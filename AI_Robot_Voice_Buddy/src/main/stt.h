#ifndef STT_H
#define STT_H

#include <Arduino.h>
#include <ArduinoWebsockets.h>

/**
 * @brief 讯飞语音听写（IAT）模块：录音上传 + 识别结果处理 + 全部意图分发
 *
 * onMessageCallback1/onEventsCallback1 为自由函数回调（ArduinoWebsockets std::function）。
 * 录音循环在 onEventsCallback1 内阻塞执行（16kHz、每帧 40ms，【时序敏感】）。
 */
// 与 STT 通信的 WebSocket 客户端（定义于 stt.cpp）
extern websockets::WebsocketsClient webSocketClient1;

// 开启一轮语音识别（含 4 分钟一次的讯飞鉴权刷新）
void StartConversation();
// 连接讯飞 IAT WebSocket 服务器
void ConnServer1();
// 直接播报 Answer 并清空（固定话术用）
void response();
// STT 识别结果回调：意图分发（唤醒词/退下/音量/开关灯/切模型/音乐/一般问答）
void onMessageCallback1(websockets::WebsocketsMessage message);
// STT 连接回调：建立后进入阻塞式录音-发送循环
void onEventsCallback1(websockets::WebsocketsEvent event, String data);

#endif // STT_H
