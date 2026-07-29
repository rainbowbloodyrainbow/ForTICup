#!/usr/bin/env python3
"""Linux viewer/recorder for the H-BALL ESP32-S3 MJPEG camera."""

from __future__ import annotations

import argparse
import json
import logging
import queue
import random
import signal
import sys
import tempfile
import threading
import time
from collections import deque
from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path
from typing import Any, Deque, Dict, Iterable, List, Optional, Tuple

import cv2
import numpy as np
import requests


APP_NAME = "H Ball Camera"
DEFAULT_STREAM_URL = "http://192.168.4.1:81/stream"
DEFAULT_STATUS_URL = "http://192.168.4.1/status"
LATEST_FRAME_QUEUE_SIZE = 2
MAX_MJPEG_BUFFER = 4 * 1024 * 1024
STATS_WINDOW_SECONDS = 5.0
STATUS_POLL_SECONDS = 2.0
RECORD_DISCONNECT_GRACE_SECONDS = 2.0

LOG = logging.getLogger("h_ball_viewer")


@dataclass(frozen=True)
class FramePacket:
    image: np.ndarray
    received_at: float
    jpeg_size: int


@dataclass
class StatsSnapshot:
    connected: bool
    backend: str
    received_frames: int
    decode_failures: int
    dropped_frames: int
    reconnects: int
    receive_fps: float
    display_fps: float
    recent_jpeg_size: int
    esp_status: Dict[str, Any] = field(default_factory=dict)


class SharedStats:
    """Thread-safe runtime counters and recent-rate samples."""

    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._connected = False
        self._backend = "starting"
        self._received_frames = 0
        self._decode_failures = 0
        self._dropped_frames = 0
        self._reconnects = 0
        self._receive_times: Deque[float] = deque(maxlen=512)
        self._display_times: Deque[float] = deque(maxlen=512)
        self._jpeg_sizes: Deque[int] = deque(maxlen=120)
        self._esp_status: Dict[str, Any] = {}

    def set_connection(self, connected: bool, backend: str) -> None:
        with self._lock:
            self._connected = connected
            self._backend = backend

    def mark_received(self, timestamp: float, jpeg_size: int) -> None:
        with self._lock:
            self._received_frames += 1
            self._receive_times.append(timestamp)
            if jpeg_size > 0:
                self._jpeg_sizes.append(jpeg_size)

    def mark_displayed(self, timestamp: float) -> None:
        with self._lock:
            self._display_times.append(timestamp)

    def mark_decode_failure(self) -> None:
        with self._lock:
            self._decode_failures += 1

    def mark_dropped(self) -> None:
        with self._lock:
            self._dropped_frames += 1

    def mark_reconnect(self) -> None:
        with self._lock:
            self._reconnects += 1

    def set_esp_status(self, status: Dict[str, Any]) -> None:
        with self._lock:
            self._esp_status = dict(status)

    @staticmethod
    def _rate(samples: Iterable[float], now: float) -> float:
        recent = [value for value in samples if now - value <= STATS_WINDOW_SECONDS]
        if len(recent) < 2:
            return 0.0
        duration = recent[-1] - recent[0]
        return 0.0 if duration <= 0 else (len(recent) - 1) / duration

    def snapshot(self) -> StatsSnapshot:
        now = time.monotonic()
        with self._lock:
            receive_fps = self._rate(self._receive_times, now)
            display_fps = self._rate(self._display_times, now)
            jpeg_size = (
                int(sum(self._jpeg_sizes) / len(self._jpeg_sizes))
                if self._jpeg_sizes
                else 0
            )
            return StatsSnapshot(
                connected=self._connected,
                backend=self._backend,
                received_frames=self._received_frames,
                decode_failures=self._decode_failures,
                dropped_frames=self._dropped_frames,
                reconnects=self._reconnects,
                receive_fps=receive_fps,
                display_fps=display_fps,
                recent_jpeg_size=jpeg_size,
                esp_status=dict(self._esp_status),
            )


class MjpegParser:
    """Bounded SOI/EOI JPEG extractor for arbitrary HTTP byte chunks."""

    SOI = b"\xff\xd8"
    EOI = b"\xff\xd9"

    def __init__(self, max_buffer: int = MAX_MJPEG_BUFFER) -> None:
        if max_buffer < 1024:
            raise ValueError("max_buffer must be at least 1024 bytes")
        self.max_buffer = max_buffer
        self.buffer = bytearray()
        self.overflow_count = 0

    def feed(self, chunk: bytes) -> List[bytes]:
        if not chunk:
            return []
        self.buffer.extend(chunk)
        frames: List[bytes] = []

        while True:
            start = self.buffer.find(self.SOI)
            if start < 0:
                self._bound_without_soi()
                break
            if start > 0:
                del self.buffer[:start]

            end = self.buffer.find(self.EOI, len(self.SOI))
            if end < 0:
                self._bound_with_partial_frame()
                break

            frame_end = end + len(self.EOI)
            frames.append(bytes(self.buffer[:frame_end]))
            del self.buffer[:frame_end]

        return frames

    def _bound_without_soi(self) -> None:
        if len(self.buffer) <= self.max_buffer:
            return
        self.overflow_count += 1
        keep = self.buffer[-1:] if self.buffer[-1:] == b"\xff" else b""
        self.buffer.clear()
        self.buffer.extend(keep)

    def _bound_with_partial_frame(self) -> None:
        if len(self.buffer) <= self.max_buffer:
            return
        self.overflow_count += 1
        # The current JPEG is unbounded/corrupt. Keep only a possible marker prefix.
        keep = self.buffer[-1:] if self.buffer[-1:] == b"\xff" else b""
        self.buffer.clear()
        self.buffer.extend(keep)


class FrameReceiver(threading.Thread):
    """Network worker which reconnects and publishes only the newest frames."""

    def __init__(
        self,
        stream_url: str,
        backend: str,
        frame_queue: "queue.Queue[FramePacket]",
        stats: SharedStats,
        stop_event: threading.Event,
        connect_timeout: float,
        read_timeout: float,
    ) -> None:
        super().__init__(name="mjpeg-receiver", daemon=False)
        self.stream_url = stream_url
        self.requested_backend = backend
        self.frame_queue = frame_queue
        self.stats = stats
        self.stop_event = stop_event
        self.connect_timeout = connect_timeout
        self.read_timeout = read_timeout

    def run(self) -> None:
        backend = "requests" if self.requested_backend == "auto" else self.requested_backend
        backoff = 0.5
        while not self.stop_event.is_set():
            self.stats.set_connection(False, backend)
            try:
                if backend == "requests":
                    received = self._requests_once()
                else:
                    received = self._opencv_once()
            except Exception:
                LOG.exception("%s receiver failed unexpectedly", backend)
                received = False

            if self.stop_event.is_set():
                break

            self.stats.set_connection(False, backend)
            self.stats.mark_reconnect()

            if self.requested_backend == "auto" and not received:
                backend = "opencv" if backend == "requests" else "requests"
                LOG.warning("No usable frame; auto backend switching to %s", backend)

            delay = 0.5 if received else backoff
            backoff = 0.5 if received else min(backoff * 2.0, 5.0)
            LOG.warning("Stream disconnected; reconnecting in %.1f s", delay)
            self.stop_event.wait(delay)

        self.stats.set_connection(False, backend)
        LOG.info("Receiver thread stopped")

    def _requests_once(self) -> bool:
        parser = MjpegParser()
        received_any = False
        last_frame_at = time.monotonic()
        session = requests.Session()
        # Local ESP32 traffic must never be sent through desktop proxy settings.
        session.trust_env = False
        response: Optional[requests.Response] = None

        try:
            LOG.info("Connecting with requests: %s", self.stream_url)
            response = session.get(
                self.stream_url,
                stream=True,
                timeout=(self.connect_timeout, self.read_timeout),
                headers={
                    "Accept": "multipart/x-mixed-replace",
                    "Accept-Encoding": "identity",
                    "Cache-Control": "no-cache",
                    "Connection": "close",
                },
            )
            response.raise_for_status()
            self.stats.set_connection(True, "requests")

            for chunk in response.iter_content(chunk_size=2048):
                if self.stop_event.is_set():
                    break
                if not chunk:
                    continue

                for jpeg in parser.feed(chunk):
                    if self.stop_event.is_set():
                        break
                    encoded = np.frombuffer(jpeg, dtype=np.uint8)
                    image = cv2.imdecode(encoded, cv2.IMREAD_COLOR)
                    if image is None or image.size == 0:
                        self.stats.mark_decode_failure()
                        continue

                    timestamp = time.monotonic()
                    last_frame_at = timestamp
                    received_any = True
                    self.stats.mark_received(timestamp, len(jpeg))
                    self._publish(FramePacket(image, timestamp, len(jpeg)))

                if time.monotonic() - last_frame_at > max(5.0, self.read_timeout * 2.0):
                    raise TimeoutError("HTTP data arrived without a complete JPEG frame")

            if not self.stop_event.is_set():
                raise ConnectionError("MJPEG HTTP response ended")
        except (requests.RequestException, TimeoutError, ConnectionError) as exc:
            if not self.stop_event.is_set():
                LOG.warning("Requests stream error: %s", exc)
        finally:
            if response is not None:
                response.close()
            session.close()
        return received_any

    def _opencv_once(self) -> bool:
        LOG.info("Connecting with OpenCV VideoCapture: %s", self.stream_url)
        capture = cv2.VideoCapture(self.stream_url)
        received_any = False
        try:
            if not capture.isOpened():
                LOG.warning("OpenCV VideoCapture could not open the stream")
                return False
            capture.set(cv2.CAP_PROP_BUFFERSIZE, 1)
            self.stats.set_connection(True, "opencv")
            consecutive_failures = 0

            while not self.stop_event.is_set():
                ok, image = capture.read()
                if not ok or image is None or image.size == 0:
                    consecutive_failures += 1
                    if consecutive_failures >= 5:
                        LOG.warning("OpenCV stream had %d consecutive read failures", consecutive_failures)
                        break
                    self.stop_event.wait(0.05)
                    continue

                consecutive_failures = 0
                received_any = True
                timestamp = time.monotonic()
                self.stats.mark_received(timestamp, 0)
                self._publish(FramePacket(image, timestamp, 0))
        finally:
            capture.release()
        return received_any

    def _publish(self, packet: FramePacket) -> None:
        try:
            self.frame_queue.put_nowait(packet)
            return
        except queue.Full:
            pass

        try:
            self.frame_queue.get_nowait()
            self.stats.mark_dropped()
        except queue.Empty:
            pass

        try:
            self.frame_queue.put_nowait(packet)
        except queue.Full:
            self.stats.mark_dropped()


class StatusPoller(threading.Thread):
    def __init__(
        self,
        status_url: str,
        stats: SharedStats,
        stop_event: threading.Event,
        timeout: float,
    ) -> None:
        super().__init__(name="esp-status-poller", daemon=False)
        self.status_url = status_url
        self.stats = stats
        self.stop_event = stop_event
        self.timeout = timeout

    def run(self) -> None:
        session = requests.Session()
        session.trust_env = False
        try:
            while not self.stop_event.is_set():
                try:
                    response = session.get(
                        self.status_url,
                        timeout=(self.timeout, self.timeout),
                        headers={"Connection": "close"},
                    )
                    response.raise_for_status()
                    data = response.json()
                    if isinstance(data, dict):
                        self.stats.set_esp_status(data)
                except (requests.RequestException, ValueError, json.JSONDecodeError) as exc:
                    LOG.debug("Optional ESP32 status poll failed: %s", exc)
                self.stop_event.wait(STATUS_POLL_SECONDS)
        finally:
            session.close()
        LOG.info("Status poller stopped")


class Recorder:
    def __init__(self, output_dir: Path, fps: float, record_overlay: bool) -> None:
        self.output_dir = output_dir
        self.fps = fps
        self.record_overlay = record_overlay
        self.writer: Optional[cv2.VideoWriter] = None
        self.path: Optional[Path] = None
        self.size: Optional[Tuple[int, int]] = None
        self.started_at = 0.0
        self.next_frame_at = 0.0
        self.frames_written = 0
        self.last_completed: Optional[Path] = None

    @property
    def active(self) -> bool:
        return self.writer is not None

    @property
    def duration(self) -> float:
        return 0.0 if not self.active else max(0.0, time.monotonic() - self.started_at)

    def start(self, image: np.ndarray) -> bool:
        if self.active:
            return True
        height, width = image.shape[:2]
        self.output_dir.mkdir(parents=True, exist_ok=True)
        stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        path = self.output_dir / f"H_BALL_CAM_{stamp}.avi"
        writer = cv2.VideoWriter(
            str(path),
            cv2.VideoWriter_fourcc(*"MJPG"),
            self.fps,
            (width, height),
        )
        if not writer.isOpened():
            writer.release()
            LOG.error("Unable to open MJPG VideoWriter: %s", path)
            return False

        now = time.monotonic()
        self.writer = writer
        self.path = path
        self.size = (width, height)
        self.started_at = now
        self.next_frame_at = now
        self.frames_written = 0
        LOG.info("Recording started: %s (%dx%d @ %.2f FPS)", path, width, height, self.fps)
        return True

    def write_due(
        self,
        clean_image: np.ndarray,
        overlay_image: np.ndarray,
        frame_received_at: float,
        connected: bool,
    ) -> None:
        if self.writer is None or self.size is None:
            return
        height, width = clean_image.shape[:2]
        if (width, height) != self.size:
            LOG.error(
                "Stream resolution changed from %s to %dx%d; stopping recording",
                self.size,
                width,
                height,
            )
            self.stop()
            return

        now = time.monotonic()
        if now - self.next_frame_at > 1.0:
            self.next_frame_at = now
        if now < self.next_frame_at:
            return

        frame_is_recent = now - frame_received_at <= RECORD_DISCONNECT_GRACE_SECONDS
        if not connected and not frame_is_recent:
            self.next_frame_at = now + 1.0 / self.fps
            return

        image = overlay_image if self.record_overlay else clean_image
        writes = 0
        while now >= self.next_frame_at and writes < 5:
            self.writer.write(image)
            self.frames_written += 1
            writes += 1
            self.next_frame_at += 1.0 / self.fps

    def stop(self) -> Optional[Path]:
        if self.writer is None:
            return self.last_completed

        path = self.path
        duration = time.monotonic() - self.started_at
        frames = self.frames_written
        self.writer.release()
        self.writer = None
        self.path = None
        self.size = None

        if path is None:
            return None
        self.last_completed = path
        file_size = path.stat().st_size if path.exists() else 0
        valid, decoded_size = verify_recording(path)
        LOG.info(
            "Recording stopped: %s | wall=%.2fs frames=%d bytes=%d reopen=%s size=%s",
            path,
            duration,
            frames,
            file_size,
            "OK" if valid else "FAILED",
            decoded_size,
        )
        return path


def verify_recording(path: Path) -> Tuple[bool, Optional[Tuple[int, int]]]:
    capture = cv2.VideoCapture(str(path))
    try:
        if not capture.isOpened():
            return False, None
        ok, frame = capture.read()
        if not ok or frame is None:
            return False, None
        height, width = frame.shape[:2]
        return True, (width, height)
    finally:
        capture.release()


def status_value(data: Dict[str, Any], *names: str) -> Any:
    for name in names:
        if name in data:
            return data[name]
    return None


def draw_overlay(
    image: np.ndarray,
    snapshot: StatsSnapshot,
    recorder: Recorder,
) -> np.ndarray:
    output = image.copy()
    height, width = output.shape[:2]
    connected_text = "CONNECTED" if snapshot.connected else "DISCONNECTED"
    recording_text = (
        f"REC {recorder.duration:6.1f}s"
        if recorder.active
        else "REC OFF"
    )
    record_name = recorder.path.name if recorder.path else "-"
    local_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-4]

    lines = [
        local_time,
        f"{connected_text} [{snapshot.backend}]  RX {snapshot.receive_fps:4.1f} FPS"
        f"  SHOW {snapshot.display_fps:4.1f} FPS",
        f"{width}x{height}  dropped={snapshot.dropped_frames}"
        f"  decode_err={snapshot.decode_failures}  reconnect={snapshot.reconnects}",
        f"{recording_text}  {record_name}",
    ]

    esp = snapshot.esp_status
    if esp:
        esp_frames = status_value(esp, "frames_sent", "frames", "total_frames")
        esp_errors = status_value(esp, "stream_errors", "errors")
        esp_fps = status_value(esp, "recent_fps", "fps")
        lines.append(f"ESP frames={esp_frames} errors={esp_errors} fps={esp_fps}")

    font = cv2.FONT_HERSHEY_SIMPLEX
    font_scale = max(0.42, min(width, height) / 1000.0)
    line_height = max(18, int(24 * font_scale / 0.48))
    box_height = line_height * len(lines) + 12
    overlay = output.copy()
    cv2.rectangle(overlay, (0, 0), (width, min(box_height, height)), (0, 0, 0), -1)
    cv2.addWeighted(overlay, 0.58, output, 0.42, 0, output)

    for index, line in enumerate(lines):
        color = (80, 255, 80)
        if "DISCONNECTED" in line:
            color = (50, 80, 255)
        if line.startswith("REC "):
            color = (80, 80, 255) if recorder.active else (220, 220, 220)
        cv2.putText(
            output,
            line,
            (8, 8 + line_height * (index + 1) - 5),
            font,
            font_scale,
            color,
            1,
            cv2.LINE_AA,
        )
    return output


def draw_waiting_frame(snapshot: StatsSnapshot, recorder: Recorder) -> np.ndarray:
    """Create a visible GUI even before the first network frame arrives."""
    image = np.full((480, 640, 3), (28, 24, 20), dtype=np.uint8)
    messages = [
        "WAITING FOR VIDEO",
        "Connect Wi-Fi: H_BALL_CAM",
        "ESP32 address: http://192.168.4.1/",
        "Press q / Esc to quit",
    ]
    font = cv2.FONT_HERSHEY_SIMPLEX
    for index, message in enumerate(messages):
        scale = 0.9 if index == 0 else 0.55
        color = (80, 180, 255) if index == 0 else (220, 220, 220)
        (text_width, _), _ = cv2.getTextSize(message, font, scale, 1)
        x = max(10, (image.shape[1] - text_width) // 2)
        y = 205 + index * 38
        cv2.putText(image, message, (x, y), font, scale, color, 1, cv2.LINE_AA)
    return draw_overlay(image, snapshot, recorder)


def save_snapshot(image: np.ndarray, directory: Path) -> Optional[Path]:
    try:
        directory.mkdir(parents=True, exist_ok=True)
        stamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")[:-3]
        path = directory / f"H_BALL_CAM_{stamp}.jpg"
        if not cv2.imwrite(str(path), image):
            LOG.error("OpenCV failed to save snapshot: %s", path)
            return None
        LOG.info("Snapshot saved: %s", path)
        return path
    except OSError as exc:
        LOG.error("Unable to save snapshot: %s", exc)
        return None


def print_shortcuts() -> None:
    print(
        "快捷键: r 开始/停止录像 | s 截图 | p 回放最近录像 | "
        "h 帮助 | q/Esc 退出"
    )


def play_file(path: Path, title: str = f"{APP_NAME} Playback") -> bool:
    capture = cv2.VideoCapture(str(path))
    if not capture.isOpened():
        LOG.error("Unable to open recording: %s", path)
        return False

    fps = capture.get(cv2.CAP_PROP_FPS)
    fps = fps if fps > 0 else 20.0
    delay_ms = max(1, int(round(1000.0 / fps)))
    paused = False
    current_frame: Optional[np.ndarray] = None
    cv2.namedWindow(title, cv2.WINDOW_NORMAL)
    LOG.info("Playing: %s (space pause, j/l ±5s, q/Esc return)", path)

    try:
        while True:
            if not paused:
                ok, frame = capture.read()
                if not ok or frame is None:
                    break
                current_frame = frame
            if current_frame is not None:
                cv2.imshow(title, current_frame)

            key = cv2.waitKey(30 if paused else delay_ms) & 0xFF
            if key in (ord("q"), 27):
                break
            if key == ord(" "):
                paused = not paused
            elif key in (ord("j"), ord("l")):
                current_ms = capture.get(cv2.CAP_PROP_POS_MSEC)
                delta = -5000.0 if key == ord("j") else 5000.0
                capture.set(cv2.CAP_PROP_POS_MSEC, max(0.0, current_ms + delta))
                ok, frame = capture.read()
                if ok and frame is not None:
                    current_frame = frame
    finally:
        capture.release()
        try:
            cv2.destroyWindow(title)
        except cv2.error:
            pass
    return True


def run_viewer(args: argparse.Namespace) -> int:
    stop_event = threading.Event()

    def request_stop(signum: int, _frame: Any) -> None:
        LOG.info("Signal %d received; shutting down", signum)
        stop_event.set()

    signal.signal(signal.SIGINT, request_stop)
    signal.signal(signal.SIGTERM, request_stop)

    frame_queue: "queue.Queue[FramePacket]" = queue.Queue(
        maxsize=LATEST_FRAME_QUEUE_SIZE
    )
    stats = SharedStats()
    receiver = FrameReceiver(
        stream_url=args.stream_url,
        backend=args.backend,
        frame_queue=frame_queue,
        stats=stats,
        stop_event=stop_event,
        connect_timeout=args.connect_timeout,
        read_timeout=args.read_timeout,
    )
    poller: Optional[StatusPoller] = None
    if not args.no_status:
        poller = StatusPoller(
            args.status_url,
            stats,
            stop_event,
            min(max(args.connect_timeout, 0.2), 5.0),
        )

    recorder = Recorder(args.output_dir, args.record_fps, args.record_overlay)
    pending_record = bool(args.record_on_start)
    latest: Optional[FramePacket] = None
    overlay_image: Optional[np.ndarray] = None
    window_created = False
    window_was_visible = False
    last_overlay_at = 0.0
    last_log_at = 0.0
    exit_code = 0

    if not args.no_display:
        try:
            cv2.namedWindow(APP_NAME, cv2.WINDOW_NORMAL)
        except cv2.error as exc:
            LOG.error(
                "OpenCV could not initialize the GUI window: %s. "
                "Use a graphical desktop session or add --no-display.",
                exc,
            )
            return 2
        window_created = True
        print_shortcuts()

    receiver.start()
    if poller is not None:
        poller.start()

    try:
        while not stop_event.is_set():
            new_frame = False
            try:
                packet = frame_queue.get(timeout=0.02)
                latest = packet
                new_frame = True
                while True:
                    try:
                        latest = frame_queue.get_nowait()
                    except queue.Empty:
                        break
            except queue.Empty:
                pass

            now = time.monotonic()
            snapshot = stats.snapshot()

            if latest is not None and pending_record and not recorder.active:
                pending_record = False
                recorder.start(latest.image)

            need_overlay = latest is not None and (
                new_frame or now - last_overlay_at >= 0.2
            )
            if need_overlay and (
                not args.no_display or args.record_overlay or recorder.active
            ):
                overlay_image = draw_overlay(latest.image, snapshot, recorder)
                last_overlay_at = now
            elif (
                latest is None
                and not args.no_display
                and now - last_overlay_at >= 0.2
            ):
                overlay_image = draw_waiting_frame(snapshot, recorder)
                last_overlay_at = now

            if recorder.active and latest is not None:
                if overlay_image is None:
                    overlay_image = latest.image
                recorder.write_due(
                    latest.image,
                    overlay_image,
                    latest.received_at,
                    snapshot.connected,
                )

            key = -1
            if not args.no_display and overlay_image is not None:
                cv2.imshow(APP_NAME, overlay_image)
                if new_frame and latest is not None:
                    stats.mark_displayed(now)
                key = cv2.waitKey(1) & 0xFF
                try:
                    visible = cv2.getWindowProperty(APP_NAME, cv2.WND_PROP_VISIBLE)
                    if visible >= 1:
                        window_was_visible = True
                    elif window_was_visible:
                        stop_event.set()
                except cv2.error:
                    stop_event.set()

            if key in (ord("q"), 27):
                stop_event.set()
            elif key == ord("r"):
                if recorder.active:
                    recorder.stop()
                elif latest is None:
                    pending_record = True
                    LOG.info("Recording will start after the first valid frame")
                else:
                    recorder.start(latest.image)
            elif key == ord("s") and latest is not None:
                save_snapshot(latest.image, args.snapshot_dir)
            elif key == ord("p"):
                if recorder.active:
                    LOG.warning("Stop recording before playback")
                elif recorder.last_completed is None:
                    LOG.warning("No completed recording is available")
                else:
                    play_file(recorder.last_completed)
            elif key == ord("h"):
                print_shortcuts()

            if now - last_log_at >= 5.0:
                LOG.info(
                    "connected=%s backend=%s rx=%.1f show=%.1f total=%d "
                    "jpeg=%dB dropped=%d decode_err=%d reconnects=%d recording=%s",
                    snapshot.connected,
                    snapshot.backend,
                    snapshot.receive_fps,
                    snapshot.display_fps,
                    snapshot.received_frames,
                    snapshot.recent_jpeg_size,
                    snapshot.dropped_frames,
                    snapshot.decode_failures,
                    snapshot.reconnects,
                    recorder.active,
                )
                last_log_at = now
    except KeyboardInterrupt:
        stop_event.set()
    except cv2.error as exc:
        LOG.error("OpenCV GUI error: %s", exc)
        stop_event.set()
        exit_code = 1
    finally:
        stop_event.set()
        recorder.stop()
        receiver.join(timeout=args.connect_timeout + args.read_timeout + 5.0)
        if receiver.is_alive():
            LOG.warning("Receiver did not stop within the expected timeout")
        if poller is not None:
            poller.join(timeout=max(args.connect_timeout, 1.0) + 3.0)
            if poller.is_alive():
                LOG.warning("Status poller did not stop within the expected timeout")
        if window_created:
            cv2.destroyAllWindows()
    return exit_code


def run_self_test() -> int:
    failures: List[str] = []
    rng = random.Random(20260729)
    width, height = 320, 240
    encoded_frames: List[bytes] = []

    for index in range(12):
        image = np.zeros((height, width, 3), dtype=np.uint8)
        image[:, :, 0] = (index * 17) % 255
        image[:, :, 1] = np.linspace(0, 255, width, dtype=np.uint8)
        cv2.putText(
            image,
            f"SELF TEST {index:02d}",
            (25, 110),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.8,
            (255, 255, 255),
            2,
            cv2.LINE_AA,
        )
        ok, encoded = cv2.imencode(".jpg", image)
        if not ok:
            failures.append(f"JPEG encode failed at frame {index}")
            continue
        encoded_frames.append(encoded.tobytes())

    multipart = bytearray()
    for jpeg in encoded_frames:
        multipart.extend(
            b"\r\n--frame\r\nContent-Type: image/jpeg\r\nContent-Length: "
            + str(len(jpeg)).encode("ascii")
            + b"\r\n\r\n"
            + jpeg
            + b"\r\n"
        )

    parser = MjpegParser(max_buffer=1024 * 1024)
    recovered: List[bytes] = []
    position = 0
    while position < len(multipart):
        size = rng.randint(1, 977)
        recovered.extend(parser.feed(bytes(multipart[position : position + size])))
        position += size
    if recovered != encoded_frames:
        failures.append(
            f"MJPEG parser recovered {len(recovered)}/{len(encoded_frames)} frames"
        )

    decoded_count = 0
    for jpeg in recovered:
        decoded = cv2.imdecode(np.frombuffer(jpeg, np.uint8), cv2.IMREAD_COLOR)
        if decoded is not None and decoded.shape[:2] == (height, width):
            decoded_count += 1
    if decoded_count != len(encoded_frames):
        failures.append(f"JPEG decode validated {decoded_count}/{len(encoded_frames)}")

    guard = MjpegParser(max_buffer=1024)
    if guard.feed(b"garbage" * 1000):
        failures.append("Garbage unexpectedly produced a JPEG")
    if guard.overflow_count == 0 or len(guard.buffer) > guard.max_buffer:
        failures.append("Buffer overflow guard did not activate")
    if guard.feed(b"\xff\xd8partial-frame"):
        failures.append("Partial JPEG unexpectedly produced a frame")

    try:
        with tempfile.TemporaryDirectory(prefix="h_ball_viewer_") as temp_dir:
            video_path = Path(temp_dir) / "self_test.avi"
            writer = cv2.VideoWriter(
                str(video_path),
                cv2.VideoWriter_fourcc(*"MJPG"),
                10.0,
                (width, height),
            )
            if not writer.isOpened():
                failures.append("MJPG VideoWriter could not open")
            else:
                for jpeg in encoded_frames:
                    frame = cv2.imdecode(
                        np.frombuffer(jpeg, np.uint8), cv2.IMREAD_COLOR
                    )
                    if frame is not None:
                        writer.write(frame)
                writer.release()
                valid, size = verify_recording(video_path)
                if not valid or size != (width, height):
                    failures.append(
                        f"Recorded AVI validation failed: valid={valid}, size={size}"
                    )
    except OSError as exc:
        failures.append(f"Temporary recording test failed: {exc}")

    if failures:
        print("SELF-TEST: FAIL")
        for failure in failures:
            print(f"  - {failure}")
        return 1

    print(
        "SELF-TEST: PASS\n"
        f"  JPEG frames: {len(encoded_frames)}\n"
        f"  Random-chunk MJPEG frames: {len(recovered)}\n"
        "  Overflow/partial-frame guard: PASS\n"
        "  AVI MJPG write/read: PASS"
    )
    return 0


def positive_float(value: str) -> float:
    parsed = float(value)
    if parsed <= 0:
        raise argparse.ArgumentTypeError("value must be greater than zero")
    return parsed


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Receive, display, record and replay H-BALL ESP32-S3 MJPEG video."
    )
    parser.add_argument("--stream-url", default=DEFAULT_STREAM_URL)
    parser.add_argument("--status-url", default=DEFAULT_STATUS_URL)
    parser.add_argument(
        "--backend",
        choices=("auto", "requests", "opencv"),
        default="requests",
    )
    parser.add_argument("--output-dir", type=Path, default=Path("recordings"))
    parser.add_argument("--snapshot-dir", type=Path, default=Path("snapshots"))
    parser.add_argument("--record-fps", type=positive_float, default=20.0)
    parser.add_argument("--record-on-start", action="store_true")
    parser.add_argument("--record-overlay", action="store_true")
    parser.add_argument("--play", type=Path, metavar="FILE")
    parser.add_argument("--no-status", action="store_true")
    parser.add_argument("--no-display", action="store_true")
    parser.add_argument("--connect-timeout", type=positive_float, default=3.0)
    parser.add_argument("--read-timeout", type=positive_float, default=10.0)
    parser.add_argument(
        "--log-level",
        choices=("DEBUG", "INFO", "WARNING", "ERROR"),
        default="INFO",
    )
    parser.add_argument("--self-test", action="store_true")
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    logging.basicConfig(
        level=getattr(logging, args.log_level),
        format="%(asctime)s.%(msecs)03d %(levelname)s %(threadName)s: %(message)s",
        datefmt="%H:%M:%S",
    )

    if args.self_test:
        return run_self_test()
    if args.play is not None:
        if args.no_display:
            LOG.error("--play requires a GUI display")
            return 2
        return 0 if play_file(args.play) else 1
    return run_viewer(args)


if __name__ == "__main__":
    sys.exit(main())
