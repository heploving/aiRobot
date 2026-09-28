#include "Audio1.h"

Audio1::Audio1()
{
  // 构造函数中初始化成员变量并创建 I2S 驱动
  wavData = nullptr;
  i2s = new I2S();
}

Audio1::~Audio1()
{
  for (int i = 0; i < wavDataSize / dividedWavDataSize; ++i)
    delete[] wavData[i];
  delete[] wavData;
  delete i2s;
}

void Audio1::init()
{
  wavData = new char *[1];
  for (int i = 0; i < 1; ++i)
    wavData[i] = new char[1280];
}

void Audio1::clear()
{
  i2s->clear();
}

void Audio1::Record()
{
  i2s->Read(i2sBuffer, i2sBufferSize);
  for (int i = 0; i < i2sBufferSize / 8; ++i)
  {
    // 麦克风信号偏弱（实测大声说话峰值仅500左右），做8倍数字增益
    int32_t s = (int32_t)(int16_t)((i2sBuffer[8 * i + 3] << 8) | i2sBuffer[8 * i + 2]) * 8;
    if (s > 32767) s = 32767;
    if (s < -32768) s = -32768;
    wavData[0][2 * i] = s & 0xFF;
    wavData[0][2 * i + 1] = (s >> 8) & 0xFF;
  }
}
