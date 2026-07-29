#include "ti_msp_dl_config.h"

// 输入命令函数
bool OLED_SendCommand(uint8_t command){
    uint8_t a[2];
    a[0] = 0x00; // 命令控制字节
    a[1] = command; // 命令字节

    // 读取 I²C Controller 的状态寄存器
    /* 以下是 I²C Controller 状态寄存器MSR的描述
    | 位 | 名称 | 掩码 | 置 1 时的含义 |
    |---:|---|---:|---|
    | 0 | `BUSY` | `0x00000001` | Controller 状态机正在执行传输 |
    | 1 | `ERR` | `0x00000002` | 地址或数据没有得到应答 |
    | 2 | `ADRACK` | `0x00000004` | 发送的地址**没有**得到 ACK |
    | 3 | `DATACK` | `0x00000008` | 发送的数据**没有**得到 ACK |
    | 4 | `ARBLST` | `0x00000010` | Controller 丢失总线仲裁 |
    | 5 | `IDLE` | `0x00000020` | Controller 状态机空闲 |
    | 6 | `BUSBSY` | `0x00000040` | I²C 物理总线正忙 |
    | 15:7 | Reserved | — | 保留 |
    | 27:16 | `MBCNT` | `0x0FFF0000` | 当前传输剩余字节计数 |
    | 31:28 | Reserved | — | 保留 |
    要判断 I2C Controller 是否处于 IDLE 状态，就是检查 bit5。
    状态值与 IDLE 掩码进行按位与：结果为 0 表示尚未空闲，
    结果非 0（此处为 0x20）表示已空闲。

    */

    // 等待 I2C Controller 进入 IDLE 状态
    while(1){
        uint32_t status = DL_I2C_getControllerStatus(OLED_I2C_INST);
        bool i2c_idle_status =
            ((status & DL_I2C_CONTROLLER_STATUS_IDLE) != 0);

        if(i2c_idle_status == true){
            break;
        }
    }

    // 将命令控制字节和命令字节填入 TX FIFO
    if(DL_I2C_fillControllerTXFIFO(OLED_I2C_INST, &a[0], 2) == 2){
        DL_I2C_startControllerTransfer(OLED_I2C_INST, 0x3C,
            DL_I2C_CONTROLLER_DIRECTION_TX, 2);
    }
    else{
        return false;
    }

    // 规避 MSPM0 I2C_ERR_13：启动后等待至少 3 个 I2C 功能时钟周期
    DL_Common_delayCycles(3);

    // 等待 I2C Controller 传输完成，即 BUSY 位变为 0
    while(1){
        uint32_t status = DL_I2C_getControllerStatus(OLED_I2C_INST);
        bool i2c_busy_status =
            ((status & DL_I2C_CONTROLLER_STATUS_BUSY) != 0);

        if(i2c_busy_status == false){
            break;
        }
    }

    // 传输结束后，分别检查应答错误和仲裁丢失
    uint32_t status = DL_I2C_getControllerStatus(OLED_I2C_INST);
    bool i2c_error_status =
        ((status & DL_I2C_CONTROLLER_STATUS_ERROR) != 0);
    bool i2c_arbitration_lost_status =
        ((status & DL_I2C_CONTROLLER_STATUS_ARBITRATION_LOST) != 0);

    if(i2c_error_status == true){
        return false;
    }

    if(i2c_arbitration_lost_status == true){
        return false;
    }

    // 确认 I2C Controller 重新进入 IDLE 状态
    while(1){
        uint32_t status = DL_I2C_getControllerStatus(OLED_I2C_INST);
        bool i2c_idle_status =
            ((status & DL_I2C_CONTROLLER_STATUS_IDLE) != 0);

        if(i2c_idle_status == true){
            break;
        }
    }

    return true;
}














int main(void){
    bool oled_status;

    // 初始化 SysConfig 配置的系统时钟、GPIO 和 I2C0
    SYSCFG_DL_init();

    // CPU 为 32 MHz，等待约 100 ms，让 OLED 上电稳定
    DL_Common_delayCycles(3200000);

    // 初始化期间先关闭显示
    oled_status = OLED_SendCommand(0xAE);
    if(oled_status == false){
        while(1){
        }
    }

    // 设置显示时钟分频比和振荡器频率
    oled_status = OLED_SendCommand(0xD5);
    if(oled_status == false){
        while(1){
        }
    }
    oled_status = OLED_SendCommand(0x80);
    if(oled_status == false){
        while(1){
        }
    }

    // 128 x 64 屏使用 1/64 驱动路数
    oled_status = OLED_SendCommand(0xA8);
    if(oled_status == false){
        while(1){
        }
    }
    oled_status = OLED_SendCommand(0x3F);
    if(oled_status == false){
        while(1){
        }
    }

    // 显示偏移设为 0
    oled_status = OLED_SendCommand(0xD3);
    if(oled_status == false){
        while(1){
        }
    }
    oled_status = OLED_SendCommand(0x00);
    if(oled_status == false){
        while(1){
        }
    }

    // 显示起始行设为 0
    oled_status = OLED_SendCommand(0x40);
    if(oled_status == false){
        while(1){
        }
    }

    // 开启 SSD1306 内部电荷泵
    oled_status = OLED_SendCommand(0x8D);
    if(oled_status == false){
        while(1){
        }
    }
    oled_status = OLED_SendCommand(0x14);
    if(oled_status == false){
        while(1){
        }
    }

    // 设置左右方向
    oled_status = OLED_SendCommand(0xA1);
    if(oled_status == false){
        while(1){
        }
    }

    // 设置上下方向
    oled_status = OLED_SendCommand(0xC8);
    if(oled_status == false){
        while(1){
        }
    }

    // 128 x 64 面板的 COM 引脚配置
    oled_status = OLED_SendCommand(0xDA);
    if(oled_status == false){
        while(1){
        }
    }
    oled_status = OLED_SendCommand(0x12);
    if(oled_status == false){
        while(1){
        }
    }

    // 设置对比度
    oled_status = OLED_SendCommand(0x81);
    if(oled_status == false){
        while(1){
        }
    }
    oled_status = OLED_SendCommand(0x7F);
    if(oled_status == false){
        while(1){
        }
    }

    // 设置预充电周期
    oled_status = OLED_SendCommand(0xD9);
    if(oled_status == false){
        while(1){
        }
    }
    oled_status = OLED_SendCommand(0xF1);
    if(oled_status == false){
        while(1){
        }
    }

    // 设置 VCOMH 取消选择级别
    oled_status = OLED_SendCommand(0xDB);
    if(oled_status == false){
        while(1){
        }
    }
    oled_status = OLED_SendCommand(0x40);
    if(oled_status == false){
        while(1){
        }
    }

    // 使用 GDDRAM 中的内容，采用正常显示极性
    oled_status = OLED_SendCommand(0xA4);
    if(oled_status == false){
        while(1){
        }
    }
    oled_status = OLED_SendCommand(0xA6);
    if(oled_status == false){
        while(1){
        }
    }

    // 开启显示
    oled_status = OLED_SendCommand(0xAF);
    if(oled_status == false){
        while(1){
        }
    }

    // 强制所有像素点亮，暂时不依赖 GDDRAM 中的内容
    oled_status = OLED_SendCommand(0xA5);
    if(oled_status == false){
        while(1){
        }
    }

    while(1){
        // SSD1306 会自动保持显示，无需重复发送命令
    }
}
