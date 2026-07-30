#include "uart_command.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "app_config.h"
#include "as5600.h"
#include "motor_control.h"
#include "ti_msp_dl_config.h"

#define RX_RING_SIZE                (128U)
#define TX_RING_SIZE                (1024U)
#define LINE_BUFFER_SIZE            (80U)

static volatile uint8_t g_rxRing[RX_RING_SIZE];
static volatile uint16_t g_rxHead;
static volatile uint16_t g_rxTail;
static volatile bool g_rxOverflow;
static uint8_t g_txRing[TX_RING_SIZE];
static uint16_t g_txHead;
static uint16_t g_txTail;
static char g_line[LINE_BUFFER_SIZE];
static uint16_t g_lineLength;

static void uart_tx_kick(void)
{
    while ((g_txTail != g_txHead) &&
           !DL_UART_Main_isTXFIFOFull(CMD_UART_INST)) {
        DL_UART_Main_transmitData(CMD_UART_INST, g_txRing[g_txTail]);
        g_txTail = (uint16_t) ((g_txTail + 1U) % TX_RING_SIZE);
    }
}

static bool uart_send_byte(uint8_t byte)
{
    uint16_t next;

    /*
     * Queueing is non-blocking so a long STATUS response cannot stall the
     * 1 kHz motor loop. The periodic control interrupt wakes main often
     * enough to keep refilling the UART hardware FIFO.
     */
    uart_tx_kick();
    next = (uint16_t) ((g_txHead + 1U) % TX_RING_SIZE);
    if (next == g_txTail) {
        return false;
    }
    g_txRing[g_txHead] = byte;
    g_txHead = next;
    return true;
}

void uart_command_send_string(const char *text)
{
    if (text == NULL) {
        return;
    }
    while (*text != '\0') {
        if (!uart_send_byte((uint8_t) *text++)) {
            return;
        }
    }
}

static void send_uint32(uint32_t value)
{
    char digits[10];
    uint32_t count = 0U;

    if (value == 0U) {
        (void) uart_send_byte((uint8_t) '0');
        return;
    }
    while ((value > 0U) && (count < sizeof(digits))) {
        digits[count++] = (char) ('0' + (value % 10U));
        value /= 10U;
    }
    while (count > 0U) {
        (void) uart_send_byte((uint8_t) digits[--count]);
    }
}

static void send_int32(int32_t value)
{
    uint32_t magnitude;

    if (value < 0) {
        (void) uart_send_byte((uint8_t) '-');
        magnitude = (uint32_t) (-(value + 1)) + 1U;
    } else {
        magnitude = (uint32_t) value;
    }
    send_uint32(magnitude);
}

static void send_float3(float value)
{
    uint32_t whole;
    uint32_t fraction;

    if (!(value == value)) {
        uart_command_send_string("nan");
        return;
    }
    if (value < 0.0f) {
        (void) uart_send_byte((uint8_t) '-');
        value = -value;
    }
    if (value > 2147483.0f) {
        uart_command_send_string("overflow");
        return;
    }

    whole = (uint32_t) value;
    fraction = (uint32_t) ((value - (float) whole) * 1000.0f + 0.5f);
    if (fraction >= 1000U) {
        whole++;
        fraction -= 1000U;
    }
    send_uint32(whole);
    (void) uart_send_byte((uint8_t) '.');
    (void) uart_send_byte((uint8_t) ('0' + fraction / 100U));
    (void) uart_send_byte((uint8_t) ('0' + (fraction / 10U) % 10U));
    (void) uart_send_byte((uint8_t) ('0' + fraction % 10U));
}

static void send_ok(const char *detail)
{
    uart_command_send_string("OK");
    if ((detail != NULL) && (*detail != '\0')) {
        uart_command_send_string(" ");
        uart_command_send_string(detail);
    }
    uart_command_send_string("\r\n");
}

static void send_error(const char *detail)
{
    uart_command_send_string("ERR ");
    uart_command_send_string(detail);
    uart_command_send_string("\r\n");
}

static bool ring_read(uint8_t *byte)
{
    if (g_rxTail == g_rxHead) {
        return false;
    }
    *byte = g_rxRing[g_rxTail];
    g_rxTail = (uint16_t) ((g_rxTail + 1U) % RX_RING_SIZE);
    return true;
}

static const char *command_argument(const char *line, const char *command)
{
    size_t length = strlen(command);
    const char *argument;

    if (strncmp(line, command, length) != 0) {
        return NULL;
    }
    argument = line + length;
    if (*argument == '\0') {
        return argument;
    }
    if ((*argument != ' ') && (*argument != '\t')) {
        return NULL;
    }
    while ((*argument == ' ') || (*argument == '\t')) {
        argument++;
    }
    return argument;
}

static bool parse_float(const char *text, float *value)
{
    bool negative = false;
    bool sawDigit = false;
    float result = 0.0f;
    float place = 0.1f;

    if ((text == NULL) || (value == NULL)) {
        return false;
    }
    if ((*text == '+') || (*text == '-')) {
        negative = (*text == '-');
        text++;
    }
    while ((*text >= '0') && (*text <= '9')) {
        sawDigit = true;
        result = result * 10.0f + (float) (*text++ - '0');
        if (result > 1000000.0f) {
            return false;
        }
    }
    if (*text == '.') {
        text++;
        while ((*text >= '0') && (*text <= '9')) {
            sawDigit = true;
            result += (float) (*text++ - '0') * place;
            place *= 0.1f;
        }
    }
    while ((*text == ' ') || (*text == '\t')) {
        text++;
    }
    if (!sawDigit || (*text != '\0')) {
        return false;
    }
    *value = negative ? -result : result;
    return true;
}

static bool parse_int(const char *text, int *value)
{
    bool negative = false;
    bool sawDigit = false;
    int result = 0;

    if ((text == NULL) || (value == NULL)) {
        return false;
    }
    if ((*text == '+') || (*text == '-')) {
        negative = (*text == '-');
        text++;
    }
    while ((*text >= '0') && (*text <= '9')) {
        sawDigit = true;
        result = result * 10 + (*text++ - '0');
        if (result > 1000) {
            return false;
        }
    }
    while ((*text == ' ') || (*text == '\t')) {
        text++;
    }
    if (!sawDigit || (*text != '\0')) {
        return false;
    }
    *value = negative ? -result : result;
    return true;
}

static void send_status(void)
{
    const MotorControlState *state = motor_control_get_state();

    uart_command_send_string("STATUS enable=");
    send_uint32(state->enabled ? 1U : 0U);
    uart_command_send_string(" calibrated=");
    send_uint32(state->calibrated ? 1U : 0U);
    uart_command_send_string(" target_deg=");
    send_float3(state->target_angle_rad * RAD_TO_DEG_F);
    uart_command_send_string(" actual_deg=");
    send_float3(state->measured_angle_rad * RAD_TO_DEG_F);
    uart_command_send_string(" velocity_rad_s=");
    send_float3(state->measured_velocity_rad_s);
    uart_command_send_string(" uq=");
    send_float3(state->uq_command_v);
    uart_command_send_string(" raw_angle=");
    send_uint32(as5600_get_raw());
    uart_command_send_string(" magnet_status=0x");
    {
        static const char hex[] = "0123456789ABCDEF";
        uint8_t status = as5600_get_status();
        (void) uart_send_byte((uint8_t) hex[(status >> 4U) & 0x0FU]);
        (void) uart_send_byte((uint8_t) hex[status & 0x0FU]);
    }
    uart_command_send_string(" driver_fault=");
    send_uint32(state->driver_fault ? 1U : 0U);
    uart_command_send_string(" fault=");
    uart_command_send_string(motor_control_fault_name(state->fault));
    uart_command_send_string(" pole_pairs=");
    send_int32(state->pole_pairs);
    uart_command_send_string(" sensor_direction=");
    send_int32(state->sensor_direction);
    uart_command_send_string(" voltage_limit=");
    send_float3(motor_control_get_voltage_limit());
    uart_command_send_string("\r\n");
}

static void execute_line(const char *line)
{
    const char *argument;
    float floatValue;
    int intValue;

    if (*line == '\0') {
        return;
    }
    if (strcmp(line, "STATUS") == 0) {
        send_status();
    } else if (strcmp(line, "HELP") == 0) {
        uart_command_send_string(
            "OK EN 0|1, CAL, ZERO, A deg, STATUS, KP x, VP x, VI x, "
            "VLIM x, VELIM x, PP n, DIR 1|-1, CLEAR, HELP\r\n");
    } else if (strcmp(line, "CAL") == 0) {
        if (motor_control_calibrate()) {
            const MotorControlState *state = motor_control_get_state();
            uart_command_send_string("OK CAL mechanical_rad=");
            send_float3(as5600_get_unwrapped_angle_rad());
            uart_command_send_string(" electrical_zero=");
            send_float3(state->electrical_zero_offset);
            uart_command_send_string(" pole_pairs=");
            send_int32(state->pole_pairs);
            uart_command_send_string(" direction=");
            send_int32(state->sensor_direction);
            uart_command_send_string("\r\n");
        } else {
            send_error("CAL failed; inspect STATUS");
        }
    } else if (strcmp(line, "ZERO") == 0) {
        motor_control_zero_here();
        send_ok("ZERO");
    } else if (strcmp(line, "CLEAR") == 0) {
        if (motor_control_clear_fault()) {
            send_ok("CLEAR");
        } else {
            send_error("hardware fault or sensor not ready");
        }
    } else if ((argument = command_argument(line, "EN")) != NULL) {
        if (!parse_int(argument, &intValue) ||
            ((intValue != 0) && (intValue != 1))) {
            send_error("EN expects 0 or 1");
        } else if (intValue == 0) {
            motor_control_enable(false);
            send_ok("EN 0");
        } else if (motor_control_try_enable()) {
            send_ok("EN 1");
        } else {
            send_error("not calibrated, sensor/fault not ready");
        }
    } else if ((argument = command_argument(line, "A")) != NULL) {
        if (parse_float(argument, &floatValue) &&
            motor_control_set_target_deg(floatValue)) {
            send_ok("A");
        } else {
            send_error("angle outside safe range");
        }
    } else if ((argument = command_argument(line, "KP")) != NULL) {
        if (parse_float(argument, &floatValue) &&
            motor_control_set_position_kp(floatValue)) {
            send_ok("KP");
        } else {
            send_error("KP range");
        }
    } else if ((argument = command_argument(line, "VP")) != NULL) {
        if (parse_float(argument, &floatValue) &&
            motor_control_set_velocity_kp(floatValue)) {
            send_ok("VP");
        } else {
            send_error("VP range");
        }
    } else if ((argument = command_argument(line, "VI")) != NULL) {
        if (parse_float(argument, &floatValue) &&
            motor_control_set_velocity_ki(floatValue)) {
            send_ok("VI");
        } else {
            send_error("VI range");
        }
    } else if ((argument = command_argument(line, "VLIM")) != NULL) {
        if (parse_float(argument, &floatValue) &&
            motor_control_set_voltage_limit(floatValue)) {
            send_ok("VLIM");
        } else {
            send_error("VLIM range");
        }
    } else if ((argument = command_argument(line, "VELIM")) != NULL) {
        if (parse_float(argument, &floatValue) &&
            motor_control_set_velocity_limit(floatValue)) {
            send_ok("VELIM");
        } else {
            send_error("VELIM range");
        }
    } else if ((argument = command_argument(line, "PP")) != NULL) {
        if (parse_int(argument, &intValue) &&
            motor_control_set_pole_pairs(intValue)) {
            send_ok("PP; recalibrate required");
        } else {
            send_error("PP range or motor enabled");
        }
    } else if ((argument = command_argument(line, "DIR")) != NULL) {
        if (parse_int(argument, &intValue) &&
            motor_control_set_sensor_direction(intValue)) {
            send_ok("DIR; recalibrate required");
        } else {
            send_error("DIR expects 1 or -1 with motor disabled");
        }
    } else {
        send_error("unknown command; use HELP");
    }
}

void uart_command_init(void)
{
    g_rxHead = 0U;
    g_rxTail = 0U;
    g_rxOverflow = false;
    g_txHead = 0U;
    g_txTail = 0U;
    g_lineLength = 0U;
}

void uart_command_handle_rx_interrupt(void)
{
    (void) DL_UART_Main_getPendingInterrupt(CMD_UART_INST);

    while (!DL_UART_Main_isRXFIFOEmpty(CMD_UART_INST)) {
        uint8_t byte =
            (uint8_t) DL_UART_Main_receiveData(CMD_UART_INST);
        uint16_t next = (uint16_t) ((g_rxHead + 1U) % RX_RING_SIZE);
        if (next == g_rxTail) {
            g_rxOverflow = true;
        } else {
            g_rxRing[g_rxHead] = byte;
            g_rxHead = next;
        }
    }
}

void uart_command_process(void)
{
    uint8_t byte;

    uart_tx_kick();
    if (g_rxOverflow) {
        g_rxOverflow = false;
        g_rxTail = g_rxHead;
        g_lineLength = 0U;
        send_error("RX overflow");
    }

    while (ring_read(&byte)) {
        if ((byte == '\r') || (byte == '\n')) {
            if (g_lineLength > 0U) {
                g_line[g_lineLength] = '\0';
                execute_line(g_line);
                g_lineLength = 0U;
            }
        } else if ((byte == '\b') || (byte == 0x7FU)) {
            if (g_lineLength > 0U) {
                g_lineLength--;
            }
        } else if ((byte >= 0x20U) && (byte <= 0x7EU)) {
            if (g_lineLength < (LINE_BUFFER_SIZE - 1U)) {
                g_line[g_lineLength++] = (char) byte;
            } else {
                g_lineLength = 0U;
                send_error("line too long");
            }
        }
    }
    uart_tx_kick();
}
