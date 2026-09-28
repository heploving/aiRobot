#ifndef MIC_I2S_H
#define MIC_I2S_H

#include <Arduino.h>
#include "driver/i2s.h"

/**
 * @brief 麦克风 I2S 驱动封装（INMP441，I2S_NUM_0，16kHz，32bit 采样）
 *
 * 引脚与采样率等参数见 config.h（MIC_I2S_* / SAMPLE_RATE_HZ）。
 * 所有参数【时序敏感】，实机验证过，勿改。
 */
class MicI2S {
public:
  MicI2S();
  int Read(char* data, int numData);   // 读满 numData 字节或超时返回；返回实际字节数
  void clear();                        // 清空 DMA 缓冲
};

#endif // MIC_I2S_H
