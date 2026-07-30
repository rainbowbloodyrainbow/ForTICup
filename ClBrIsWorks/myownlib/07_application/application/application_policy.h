#ifndef APPLICATION_POLICY_H
#define APPLICATION_POLICY_H

#include <stdint.h>

/*
 * 当前巡迹只允许两轮前进或单侧停转。通用 chassis 仍保留有符号差速能力，
 * 这一函数只负责将巡迹转向量限制在基础前进输出能够覆盖的范围内。
 */
int16_t ApplicationPolicy_LimitTurnForForwardDrive(
    int16_t driveOutput, int16_t turnOutput);

#endif
