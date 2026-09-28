#ifndef LLM_H
#define LLM_H

/**
 * @file    llm.h
 * @brief   大模型对话模块接口：讯飞星火 WebSocket 会话 + 豆包/通义/ChatGPT HTTP-SSE 流式调用 + 对话历史
 *
 * 共享状态（Answer/subAnswers/text 等）定义于 main.cpp、声明于 app.h。
 * onMessageCallback/onEventsCallback 为自由函数回调（ArduinoWebsockets std::function）。
 */
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoWebsockets.h>

// 与 LLM 通信的 WebSocket 客户端（定义于 llm.cpp）
extern websockets::WebsocketsClient webSocketClient;

// 连接讯飞星火 WebSocket 服务器
void ConnServer();
// 星火 WS 消息回调：流式回复清洗后追加到 Answer
void onMessageCallback(websockets::WebsocketsMessage message);
// 星火 WS 事件回调：连接建立时发送对话参数
void onEventsCallback(websockets::WebsocketsEvent event, String data);

// 生成讯飞星火 WebSocket 请求 JSON
DynamicJsonDocument gen_params(const char *appid, const char *domain, const char *role_set);
// 生成 OpenAI 风格 HTTP 请求 JSON（豆包/通义/ChatGPT 共用）
DynamicJsonDocument gen_params_http(const char *model, const char *role_set);
// 将 Answer 按句读断句，逐段入队播放
void processResponse(int status);

// 对话历史管理
void getText(String role, String content, bool show = true);
void checkLen();
// 按 UTF-8 整字符边界截断：返回不超过 maxBytes 的最大字节位置（新增，供强制截断使用）
int trimToUtf8Boundary(const String &s, int maxBytes);

// 讯飞鉴权：从百度响应头取时间 + HMAC-SHA256 生成鉴权 URL
void getTimeFromServer();
String getUrl(String sparkUrl, String host, String path, String date);

// 工具：过滤 LLM 回复中的无用符号
void removeChars(const char *input, char *output, const char *removeSet);

// 豆包/通义/ChatGPT 阻塞式 SSE 流式请求
void doubao();
void tongyi();
void chatgpt();

#endif // LLM_H
