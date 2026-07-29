/**
 * @file    oled.h
 * @brief   SSD1306 OLED 128x64 显示器驱动 (I2C0)
 *
 * 赛题要求屏 ≤ 2 英寸。0.96" SSD1306 (128×64) 满足要求。
 *
 * 引脚:
 *   I2C0 SCL → PA2 (PINCM2)
 *   I2C0 SDA → PA3 (PINCM3)
 *
 * I2C 地址：0x3C (SSD1306 默认, SA0=0)
 */

#ifndef OLED_H_
#define OLED_H_

#include "ti_msp_dl_config.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===== 硬件引脚 ===== */
#define OLED_I2C            I2C0
#define OLED_I2C_SCL_PINCM  (IOMUX_PINCM6)
#define OLED_I2C_SDA_PINCM  (IOMUX_PINCM3)

#define OLED_I2C_ADDR       0x3C
#define OLED_I2C_SPEED      400000  /* 400kHz Fast Mode */

/* 屏幕尺寸 */
#define OLED_WIDTH          128
#define OLED_HEIGHT         64
#define OLED_PAGES          8        /* 64 / 8 = 8 pages */

/* ===== 公共接口 ===== */

void oled_init(void);
void oled_clear(void);
void oled_update(void);
void oled_update_page(uint8_t page);
void oled_string(uint8_t x, uint8_t page, const char *str);
void oled_pixel(uint8_t x, uint8_t y, bool on);
void oled_hline(uint8_t x, uint8_t y, uint8_t w);
void oled_vline(uint8_t x, uint8_t y, uint8_t h);
void oled_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool on);
void oled_big_number(int16_t n, uint8_t x, uint8_t page);

/**
 * @brief 显示赛题默认信息布局 (128x64):
 *   Page 0-1: 任务状态 / 单圈时间
 *   Page 2-3: Ball: xxx  (球当前位置)
 *   Page 4-5: Target: xxx (目标位置)
 *   Page 6-7: 速度 / 传感器状态
 */
void oled_display_status(const char *task, int16_t ball_pos, int16_t target,
                         int16_t speed, uint32_t lap_time_ms);

#ifdef __cplusplus
}
#endif

#endif /* OLED_H_ */
