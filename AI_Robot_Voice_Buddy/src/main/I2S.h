#ifndef _I2S_H
#define _I2S_H
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "esp_system.h"

class I2S {
public:
  I2S();
  int Read(char* data, int numData);
  void clear();
};

#endif // _I2S_H
