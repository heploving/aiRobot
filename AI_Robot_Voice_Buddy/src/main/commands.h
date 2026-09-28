#ifndef COMMANDS_H
#define COMMANDS_H

#include <Arduino.h>

/**
 * @brief 语音指令处理模块：数字提取、音量调节
 */
// 提取字符串中的数字
String extractNumber(const String &str);
// 音量指令分发（识别 askquestion 中的意图）
void VolumeSet();
// 钳制音量到 [0, VOLUME_MAX] 并应用（B6 修复）
void setVolumeTo(int vol);

#endif // COMMANDS_H
