/**
 * @file    commands.cpp
 * @brief   语音指令处理模块：数字提取、音量调节
 *
 * 共享状态（volume/askquestion/conflag 等）定义于 main.cpp、声明于 app.h。
 */
#include "commands.h"
#include "config.h"
#include "app.h"
#include "display.h"

// 提取字符串中的数字
String extractNumber(const String &str) {
  String result;
  for (size_t i = 0; i < str.length(); i++) {
    if (isDigit(str[i])) {
      result += str[i];
    }
  }
  return result;
}

// 钳制音量到 [0, VOLUME_MAX] 并应用
// （B6 修复：原实现对"音量 999"等指令不钳制，int 转 uint8_t 回绕成 231）
void setVolumeTo(int vol)
{
    volume = constrain(vol, 0, VOLUME_MAX);
    audio2.setVolume(volume);
    Serial.print("音量已调到: ");
    Serial.println(volume);
    showVolume(volume);
}

// 音量控制
void VolumeSet()
{
    String numberStr = extractNumber(askquestion);
    // 显示当前音量（声音）或者当前音量（声音）是多少
    if ((askquestion.indexOf("显示") > -1 && askquestion.indexOf("音") > -1) || (askquestion.indexOf("音") > -1 && askquestion.indexOf("多") > -1))
    {
        Serial.print("当前音量为: ");
        Serial.println(volume);
        // 在屏幕上显示音量
        showVolume(volume);
    }
    else if (numberStr.length() > 0)
    {
        setVolumeTo(numberStr.toInt());   // B6 修复：钳制后应用
    }
    else if (askquestion.indexOf("最") > -1 && (askquestion.indexOf("高") > -1 || askquestion.indexOf("大") > -1))
    {
        setVolumeTo(VOLUME_MAX);
    }
    else if (askquestion.indexOf("高") > -1 || askquestion.indexOf("大") > -1)
    {
        setVolumeTo(volume + VOLUME_STEP);
    }
    else if (askquestion.indexOf("最") > -1 && (askquestion.indexOf("低") > -1 || askquestion.indexOf("小") > -1))
    {
        setVolumeTo(0);
    }
    else if (askquestion.indexOf("低") > -1 || askquestion.indexOf("小") > -1)
    {
        setVolumeTo(volume - VOLUME_STEP);
    }
    conflag = 1;
}
