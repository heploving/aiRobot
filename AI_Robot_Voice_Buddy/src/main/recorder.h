#ifndef RECORDER_H
#define RECORDER_H

#include <Arduino.h>
#include "config.h"
#include "mic_i2s.h"

/**
 * @brief 录音模块：INMP441 麦克风 I2S 采集
 *
 * 输出 16bit 单声道 16kHz 线性 PCM，每帧 FRAME_BYTES=1280 字节（40ms）。
 * 帧长与 MIC_GAIN 增益【时序敏感】，实机验证过，勿改。
 * 帧数据经 main.cpp 计算 RMS（静音检测）并 Base64 后发送讯飞 IAT。
 */
class Recorder
{
  MicI2S *i2s;
  static const int kI2sBufferSize = I2S_BUF_SIZE;   // 一次 I2S 读入的原始字节数（32bit×双声道，40ms）
  char i2sBuffer[kI2sBufferSize];
  int wavChunkCount = 0;    // 实际分配的缓冲块数，析构按此删除（防越界）

public:
  char **wavData;           // 分段存储。因为在ESP32中无法分配大块连续内存区域。

  Recorder();
  ~Recorder();
  bool init();              // 分配录音缓冲（重复调用无副作用）
  bool Record();            // 采集一帧；返回是否读满（异常时保持上一帧数据，由上层按原节奏续发）
  void clear();             // 清空 I2S DMA 缓冲

  // 推荐通过 frame() 访问当前帧缓冲，替代直接操作 wavData[0]
  const char *frame() const { return wavData[0]; }
};

// 全局录音对象（定义于 main.cpp）
extern Recorder recorder;

// 计算 PCM 帧 RMS 值（噪声门限/静音检测用，16bit 小端样本）
float calculateRMS(uint8_t *buffer, int bufferSize);

#endif // RECORDER_H
