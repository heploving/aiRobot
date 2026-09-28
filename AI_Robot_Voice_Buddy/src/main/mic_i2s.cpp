#include "mic_i2s.h"
#include "config.h"

// This I2S specification :
//  -   LRC high is channel 2 (right).
//  -   LRC signal transitions once each word.
//  -   DATA is valid on the CLOCK rising edge.
//  -   Data bits are MSB first.
//  -   DATA bits are left-aligned with respect to LRC edge.
//  -   DATA bits are right-shifted by one with respect to LRC edges.
MicI2S::MicI2S()
{
  const i2s_bits_per_sample_t bitsPerSample = I2S_BITS_PER_SAMPLE_32BIT;
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = SAMPLE_RATE_HZ,
      .bits_per_sample = bitsPerSample,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
      .intr_alloc_flags = 0,
      .dma_buf_count = 16,
      .dma_buf_len = 60};
  i2s_pin_config_t pin_config;
  pin_config.bck_io_num = MIC_I2S_BCLK;
  pin_config.ws_io_num = MIC_I2S_LRC;
  pin_config.data_out_num = I2S_PIN_NO_CHANGE;
  pin_config.data_in_num = MIC_I2S_DIN;
  pin_config.mck_io_num = I2S_PIN_NO_CHANGE; // INMP441 不需要 MCLK，GPIO0 在 S3 上会导致引脚配置失败
  esp_err_t err;
  err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  if (err != ESP_OK) { log_e("i2s_driver_install failed: %s", esp_err_to_name(err)); }
  err = i2s_set_pin(I2S_NUM_0, &pin_config);
  if (err != ESP_OK) { log_e("i2s_set_pin failed: %s", esp_err_to_name(err)); }
  err = i2s_set_clk(I2S_NUM_0, SAMPLE_RATE_HZ, bitsPerSample, I2S_CHANNEL_STEREO);
  if (err != ESP_OK) { log_e("i2s_set_clk failed: %s", esp_err_to_name(err)); }
}

int MicI2S::Read(char *data, int numData)
{
  size_t bytesRead = 0;
  // 带超时读取：正常 40ms 满帧永不触发，仅在 I2S 外设异常时防止永久阻塞 → 看门狗复位
  i2s_read(I2S_NUM_0, (char *)data, numData, &bytesRead, pdMS_TO_TICKS(MIC_READ_TIMEOUT_MS));
  return (int)bytesRead;
}

void MicI2S::clear()
{
  i2s_zero_dma_buffer(I2S_NUM_0);
}
