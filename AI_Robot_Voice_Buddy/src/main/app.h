#ifndef APP_H
#define APP_H

/**
 * @file    app.h
 * @brief   跨模块接口中枢：全部全局状态 extern + 协调函数声明
 *
 * 各全局变量的定义分散在 main.cpp（对话/播放状态）与各模块（见注释），
 * 这里集中声明供所有模块使用。本头文件不 include 任何模块头，避免循环依赖。
 */
#include <Arduino.h>
#include <vector>
#include "Audio2.h"
#include <Preferences.h>

// —— 设备对象 ——
extern Audio2 audio2;            // 定义于 main.cpp
extern Preferences preferences;  // 定义于 main.cpp（应用级 NVS 存储）

// —— LLM 选择（0豆包 1星火 2通义 3ChatGPT）——
extern int llm;

// —— 播放调度 ——
extern bool startPlay;
extern bool ledstatus;

// —— 讯飞鉴权 ——
extern unsigned long urlTime;
extern String url, url1, Date;

// —— 录音/噪声 ——
extern int noise;
extern float lastAmbient;

// —— 音量 ——
extern int volume;

// —— 音乐 ——
extern int mainStatus, conStatus, musicnum, musicplay, cursorY;

// —— 对话状态 ——
extern int awake_flag, await_flag, start_con, conflag, flag;
extern std::vector<String> text;      // 对话历史（JSON 字符串，一问一答成对存储）
extern String askquestion;            // 当前用户提问（STT 结果/指令文本）
extern String Answer;                 // LLM 回答缓冲
extern std::vector<String> subAnswers; // 长回答分段
extern int subindex;                  // subAnswers 的下标，用于 voicePlay()
extern String text_temp;              // displayWrappedText 换屏残留文字

// —— 打字机同步状态机 ——
extern String syncText;               // 当前播报中、需同步显示的文字段
extern int syncTotalMs, syncShownBytes;
extern bool syncStarted;
extern unsigned long syncStartMs, syncReqMs;

// —— 跨模块协调函数 ——
void voicePlay();                    // 播放下一段分段回答/音乐，定义于 main.cpp
void speakAndDisplay(String text);   // 发起 TTS 播报并启动文字同步显示，定义于 main.cpp
void response();                     // 直接播报 Answer 并清空，定义于 main.cpp
bool keyPressed();                   // boot 键消抖检测（500ms 节流），定义于 main.cpp
void setVolumeTo(int vol);           // 钳制音量并应用，定义于 commands.cpp

#endif // APP_H
