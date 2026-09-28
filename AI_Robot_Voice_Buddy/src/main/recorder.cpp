#include "recorder.h"
#include <math.h>

namespace {
// I2S 32bit 帧中每声道占 4 字节（槽），麦克风数据落在槽内字节 2-3（bit[23:8]）
constexpr int kBytesPerSampleSlot = 4;
constexpr int kSampleByteOffset = 2;
// 双声道一次处理 8 字节 = 2 帧，取第 1 帧声道数据
constexpr int kBytesPerFramePair = kBytesPerSampleSlot * 2;
} // namespace

Recorder::Recorder()
{
  wavData = nullptr;
  i2s = new MicI2S();
}

Recorder::~Recorder()
{
  // 按实际分配块数删除（旧实现按 wavDataSize/dividedWavDataSize=23 块删，
  // 而 init 只分配 1 块，越界 delete 属未定义行为）
  if (wavData)
  {
    for (int i = 0; i < wavChunkCount; ++i)
      delete[] wavData[i];
    delete[] wavData;
  }
  delete i2s;
}

bool Recorder::init()
{
  if (wavData != nullptr)
    return true;    // 已初始化，避免重复分配泄漏
  wavData = new char *[1];
  // 【时序敏感】每帧 FRAME_BYTES=1280 字节 = 16kHz×2B×40ms，实机验证，勿改
  wavData[0] = new char[FRAME_BYTES];
  wavChunkCount = 1;
  return true;
}

void Recorder::clear()
{
  i2s->clear();
}

bool Recorder::Record()
{
  // 【时序敏感】一次读 5120 字节 = 40ms 原始帧（16kHz×32bit×双声道），实机验证，勿改
  if (i2s->Read(i2sBuffer, kI2sBufferSize) < kI2sBufferSize)
    return false;   // 读超时/异常：保持上一帧数据，由上层按原节奏继续发送

  for (int i = 0; i < kI2sBufferSize / kBytesPerFramePair; ++i)
  {
    // 【时序敏感】麦克风信号偏弱（实测大声说话峰值仅 ~500），做 MIC_GAIN 倍数字增益；
    // (int16_t) 截断取 INMP441 左对齐 24bit 数据的低 16 位，依赖 char 符号扩展，勿改
    int32_t s = (int32_t)(int16_t)((i2sBuffer[kBytesPerFramePair * i + kSampleByteOffset + 1] << 8) |
                                   i2sBuffer[kBytesPerFramePair * i + kSampleByteOffset]) *
                MIC_GAIN;
    if (s > 32767) s = 32767;
    if (s < -32768) s = -32768;
    wavData[0][2 * i] = s & 0xFF;
    wavData[0][2 * i + 1] = (s >> 8) & 0xFF;
  }
  return true;
}

float calculateRMS(uint8_t *buffer, int bufferSize)
{
  float sum = 0;
  int16_t sample;

  // 每次处理两个字节，16位
  for (int i = 0; i < bufferSize; i += 2)
  {
    // 从缓冲区中读取16位样本，注意字节顺序（小端）
    sample = (buffer[i + 1] << 8) | buffer[i];

    // 计算平方和
    sum += sample * sample;
  }

  // 计算平均值
  sum /= (bufferSize / 2);

  // 返回RMS值
  return sqrt(sum);
}
