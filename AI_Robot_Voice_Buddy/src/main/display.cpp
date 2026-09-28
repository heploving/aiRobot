#include "display.h"
#include "config.h"
#include <Wire.h>

// 屏幕对象（全局唯一，各模块通过 display.h 的 extern 使用）
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

// 超出当前屏幕的文字，在下一屏幕显示（定义于 main.cpp）
extern String text_temp;

void displayInit()
{
    Wire.begin(OLED_SDA, OLED_SCL);     // OLED I2C（引脚见 config.h）
    // 扫描 I2C 总线，自动适配 OLED 地址（0x3C 或 0x3D）
    {
      bool addr3D = false;
      Serial.println("I2C scan:");
      for (uint8_t addr = 1; addr < 127; addr++)
      {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0)
        {
          Serial.printf("  found 0x%02X\n", addr);
          if (addr == 0x3D) addr3D = true;
        }
      }
      if (addr3D) u8g2.setI2CAddress(0x3D << 1);
    }
    u8g2.begin();                        // SSD1306 0.96寸 OLED

    // 初始化U8g2
    u8g2.setFont(u8g2_font_wqy12_t_gb2312); // 12px UTF-8 中文字体（GB2312全集7533字，chinese3子集缺常用字如"待"）
    u8g2.enableUTF8Print();                     // 启用 UTF-8 打印
    u8g2.setFontMode(1);                    // 设置字体模式为透明模式，不设置的话中文字符会变成一个黑色方块
    u8g2.setDrawColor(1);                   // 单色屏: 1=点亮
    // 显示文字
    u8g2.setCursor(0, 11);
    u8g2.print("已开机！");
    u8g2.sendBuffer();
}

// 自动换行显示u8g2文本的函数
void displayWrappedText(const std::string &text1, int x, int y, int maxWidth)
{
    int cursorX = x;
    int cursorY = y;
    int lineHeight = u8g2.getFontAscent() - u8g2.getFontDescent() + 2; // 中文字符12像素高度
    int start = 0;                                                     // 指示待输出字符串已经输出到哪一个字符
    int num = text1.size();
    int i = 0;

    while (start < num)
    {
        u8g2.setCursor(cursorX, cursorY);
        int wid = 0;
        int numBytes = 0;

        // Calculate how many bytes fit in the maxWidth
        while (i < num)
        {
            int size = 1;
            if (text1[i] & 0x80)
            { // 核心所在
                char temp = text1[i];
                temp <<= 1;
                do
                {
                    temp <<= 1;
                    ++size;
                } while (temp & 0x80);
            }
            std::string subWord;
            subWord = text1.substr(i, size); // 取得单个中文或英文字符

            int charBytes = subWord.size(); // 获取字符的字节长度

            int charWidth = charBytes == UTF8_CJK_BYTES ? CHAR_W_CJK : CHAR_W_ASCII; // 中文字符12像素宽度，英文字符6像素宽度
            if (wid + charWidth > maxWidth - cursorX)
            {
                break;
            }
            numBytes += charBytes;
            wid += charWidth;

            i += size;
        }

        if (cursorY <= SCREEN_HEIGHT - 10)
        {
            u8g2.print(text1.substr(start, numBytes).c_str());
            cursorY += lineHeight;
            cursorX = 0;
            start += numBytes;
        }
        else
        {
            text_temp = text1.substr(start).c_str();
            break;
        }
    }
    u8g2.sendBuffer();
}

// 返回位置 i 处 UTF-8 字符占用的字节数（1~4）
int utf8CharSize(const char *s, int i)
{
    int size = 1;
    if (s[i] & 0x80)
    {
        char temp = s[i];
        temp <<= 1;
        do
        {
            temp <<= 1;
            ++size;
        } while (temp & 0x80);
    }
    return size;
}

// 单个 UTF-8 字符的预估播报时长（毫秒），按百度TTS spd=6 语速估算，可按实测微调
int charMs(String text, int i)
{
    int size = utf8CharSize(text.c_str(), i);
    int ms = size == UTF8_CJK_BYTES ? CHAR_MS_CJK : CHAR_MS_ASCII;   // 中文每字约190ms，ASCII约90ms
    if (size == UTF8_CJK_BYTES)
    {
        const char *p = text.c_str() + i;
        if (!strncmp(p, "。", 3) || !strncmp(p, "！", 3) || !strncmp(p, "？", 3) ||
            !strncmp(p, "；", 3) || !strncmp(p, "，", 3) || !strncmp(p, "、", 3) ||
            !strncmp(p, "：", 3) || !strncmp(p, "…", 3))
            ms += PUNCT_PAUSE_MS;   // 标点附加停顿
    }
    return ms;
}

// 估算一段文字用百度TTS播报的总时长（毫秒）
int speechMs(String text)
{
    int ms = 0;
    for (int i = 0; i < text.length(); i += utf8CharSize(text.c_str(), i))
        ms += charMs(text, i);
    return ms;
}

// 按语音进度换算应显示的字节数（截断到 UTF-8 整字符边界）
int bytesForMs(String text, int ms)
{
    int acc = 0;
    int i = 0;
    while (i < text.length())
    {
        acc += charMs(text, i);
        i += utf8CharSize(text.c_str(), i);
        if (acc >= ms)
            return i;
    }
    return text.length();
}

// 打字机式显示：只显示文字前 showBytes 字节，超出屏幕时滚动显示末尾几行
void displayProgress(String text, int showBytes)
{
    u8g2.clearBuffer();
    if (showBytes <= 0)
    {
        u8g2.sendBuffer();
        return;
    }
    if (showBytes > text.length())
        showBytes = text.length();

    int lineHeight = u8g2.getFontAscent() - u8g2.getFontDescent() + 2;
    int startY = 11;   // 与 displayWrappedText 保持一致
    int maxLines = 0;
    for (int y = startY; y <= SCREEN_HEIGHT - 10; y += lineHeight)
        maxLines++;

    // 第一遍：按屏宽换行，记录每行起点（字节偏移）
    // 环形窗口：只保留最近 kMaxTrackedLines 个换行点（maxLines ≤ 5，8 足够），避免长文本栈越界
    const int kMaxTrackedLines = 8;
    int lineStarts[kMaxTrackedLines];
    int lineCount = 0;
    int i = 0;
    int wid = 0;
    lineStarts[lineCount++ % kMaxTrackedLines] = 0;
    while (i < showBytes)
    {
        int size = utf8CharSize(text.c_str(), i);
        int charWidth = size == UTF8_CJK_BYTES ? CHAR_W_CJK : CHAR_W_ASCII;
        if (wid + charWidth > SCREEN_WIDTH)
        {
            lineStarts[lineCount++ % kMaxTrackedLines] = i;
            wid = 0;
        }
        wid += charWidth;
        i += size;
    }

    // 若超过一屏，只显示最后 maxLines 行（跟随语音的滚动窗口）
    int pos = (lineCount > maxLines) ? lineStarts[(lineCount - maxLines) % kMaxTrackedLines] : 0;

    // 第二遍：从窗口起点按行打印
    int y = startY;
    while (pos < showBytes && y <= SCREEN_HEIGHT - 10)
    {
        int lineBytes = 0;
        int w = 0;
        int j = pos;
        while (j < showBytes)
        {
            int size = utf8CharSize(text.c_str(), j);
            int charWidth = size == UTF8_CJK_BYTES ? CHAR_W_CJK : CHAR_W_ASCII;
            if (w + charWidth > SCREEN_WIDTH)
                break;
            w += charWidth;
            lineBytes += size;
            j += size;
        }
        if (lineBytes == 0)
            break;
        u8g2.setCursor(0, y);
        u8g2.print(text.substring(pos, pos + lineBytes).c_str());   // substring(起, 止) 第二参数是结束位置
        y += lineHeight;
        pos += lineBytes;
    }
    u8g2.sendBuffer();
}

// 在屏幕右上角显示当前音量
void showVolume(int vol)
{
    u8g2.setCursor(VOLUME_DISP_X, 0);
    u8g2.print("音量:");
    u8g2.print(vol);
    u8g2.sendBuffer();
}

// 显示一行（可选两行）配置状态提示
void showConfigStatus(const char *line1, const char *line2)
{
    u8g2.clearBuffer();
    u8g2.setCursor(0, 11);
    u8g2.print(line1);
    if (line2)
    {
        u8g2.setCursor(0, u8g2.getCursorY() + 12);
        u8g2.print(line2);
    }
    u8g2.sendBuffer();
}
