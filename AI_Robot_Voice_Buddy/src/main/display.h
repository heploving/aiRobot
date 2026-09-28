#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <string>
#include <U8g2lib.h>

/**
 * @brief 屏幕显示模块：SSD1306 0.96 寸 OLED（I2C）全部显示逻辑
 *
 * 全局对象 u8g2 定义于 display.cpp；屏幕参数见 config.h（OLED_SDA/OLED_SCL、
 * SCREEN_WIDTH/SCREEN_HEIGHT）。中文字体为 GB2312 全集 12px（u8g2_font_wqy12_t_gb2312）。
 */
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

// 初始化屏幕：I2C 地址自动扫描（0x3C/0x3D）、字体设置、开机画面
void displayInit();
// 自动换行显示 UTF-8 文本；超出屏幕部分存入 text_temp 供下一屏显示
void displayWrappedText(const std::string &text1, int x, int y, int maxWidth);
// 返回位置 i 处 UTF-8 字符占用的字节数（1~4）
int utf8CharSize(const char *s, int i);
// 单个 UTF-8 字符的预估播报时长（毫秒）
int charMs(String text, int i);
// 估算一段文字用百度 TTS 播报的总时长（毫秒）
int speechMs(String text);
// 按语音进度换算应显示的字节数（截断到 UTF-8 整字符边界）
int bytesForMs(String text, int ms);
// 打字机式显示：只显示文字前 showBytes 字节，超出屏幕时滚动显示末尾几行
void displayProgress(String text, int showBytes);
// 在屏幕右上角显示当前音量
void showVolume(int vol);
// 显示一行（可选两行）配置状态提示
void showConfigStatus(const char *line1, const char *line2 = nullptr);

#endif // DISPLAY_H
