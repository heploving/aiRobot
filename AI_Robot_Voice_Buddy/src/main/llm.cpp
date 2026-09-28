/**
 * @file    llm.cpp
 * @brief   大模型对话模块：讯飞星火 WebSocket 会话 + 豆包/通义/ChatGPT HTTP-SSE 流式调用 + 对话历史管理
 *
 * 共享状态（Answer/subAnswers/text 等）定义于 main.cpp、声明于 app.h。
 * onMessageCallback/onEventsCallback 为自由函数回调（ArduinoWebsockets std::function）。
 *
 * 时序约定：
 *   - 讯飞鉴权 URL 4 分钟过期（AUTH_REFRESH_MS），HMAC-SHA256 签名依赖 Date 头
 *   - 回答分段阈值 SEGMENT_SHORT_BYTES=180 / SEGMENT_LONG_BYTES=300（UTF-8 整字符边界截断）
 */
#include "llm.h"
#include "config.h"
#include "app.h"
#include "display.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include "base64.h"
#include <mbedtls/md.h>

using namespace websockets;

// 与 LLM 通信的 WebSocket 客户端
WebsocketsClient webSocketClient;

void getText(String role, String content, bool show)
{
    // 创建一个静态JSON文档（容量过小会静默截断历史，B4 修复：512→HISTORY_JSON_DOC_BYTES）
    StaticJsonDocument<HISTORY_JSON_DOC_BYTES> jsoncon;

    // 设置JSON文档中的角色和内容
    jsoncon["role"] = role;
    jsoncon["content"] = content;
    Serial.print("jsoncon：");
    Serial.println(jsoncon.as<String>());

    // 将JSON文档序列化为字符串
    String jsonString;
    size_t serialized = serializeJson(jsoncon, jsonString);
    if (serialized == 0)
    {
        // 序列化失败（容量不足）：本条不存入，避免历史出现不完整条目（B4 修复）
        Serial.println("WARN: 对话历史序列化失败，本条不存入");
        return;
    }

    // 将字符串存储到vector中
    text.push_back(jsonString);

    // 先存后清：保证最新一问一答永远不会被清理
    // （B5 修复：原实现先 checkLen 再 push，单条超长时会把刚加入的问题删掉）
    checkLen();

    // 输出vector中的内容
    for (const auto& jsonStr : text) {
        Serial.println(jsonStr);
    }

    // 清空临时JSON文档
    jsoncon.clear();

    // 打印角色和内容（assistant 的文字改由 speakAndDisplay 随语音同步显示）
    if (show)
    {
        // 整段显示提问，超过一屏时滚动显示末尾（与回答一致，不再截断丢字）
        String line = role + ": " + content;
        displayProgress(line, line.length());
    }

}

// 实时清理较早的历史对话记录
void checkLen()
{
    size_t totalBytes = 0;

    // 计算vector中每个字符串的长度
    for (const auto& jsonStr : text) {
        totalBytes += jsonStr.length();
    }
    Serial.print("text size:");
    Serial.println(totalBytes);
    // 超过 HISTORY_MAX_BYTES 字节时，成对删除最旧的一问一答；
    // 只删到剩最后一对为止，最新条目永不删除（B5 修复）
    while (totalBytes > HISTORY_MAX_BYTES && text.size() > 2)
    {
        Serial.println("totalBytes大于800,删除最开始的一对对话");
        totalBytes -= text[0].length() + text[1].length();
        text.erase(text.begin(), text.begin() + 2);
    }
}

// 按 UTF-8 整字符边界截断：返回不超过 maxBytes 的最大字节位置
// （B1 修复的辅助函数：强制截断长回答时避免切断多字节字符）
int trimToUtf8Boundary(const String &s, int maxBytes)
{
    if (s.length() <= maxBytes) return s.length();
    int pos = maxBytes;
    // 若落在 UTF-8 后续字节（10xxxxxx）上，回退到该字符的起始字节
    while (pos > 0 && ((uint8_t)s[pos] & 0xC0) == 0x80) pos--;
    return pos;
}

DynamicJsonDocument gen_params(const char *appid, const char *domain, const char *role_set)
{
    // 创建一个容量为1500字节的动态JSON文档
    DynamicJsonDocument data(1500);

    // 创建一个名为header的嵌套JSON对象，并添加app_id和uid字段
    JsonObject header = data.createNestedObject("header");
    header["app_id"] = appid;
    header["uid"] = "1234";

    // 创建一个名为parameter的嵌套JSON对象
    JsonObject parameter = data.createNestedObject("parameter");

    // 在parameter对象中创建一个名为chat的嵌套对象，并添加domain, temperature和max_tokens字段
    JsonObject chat = parameter.createNestedObject("chat");
    chat["domain"] = domain;
    chat["temperature"] = 0.6;
    chat["max_tokens"] = 1024;

    // 创建一个名为payload的嵌套JSON对象
    JsonObject payload = data.createNestedObject("payload");

    // 在payload对象中创建一个名为message的嵌套对象
    JsonObject message = payload.createNestedObject("message");

    // 在message对象中创建一个名为text的嵌套数组
    JsonArray textArray = message.createNestedArray("text");

    JsonObject systemMessage = textArray.createNestedObject();
    systemMessage["role"] = "system";
    systemMessage["content"] = role_set;

    // 遍历全局变量text中的每个元素，并将其添加到text数组中
    /*for (const auto &item : text)
    {
        textArray.add(item);
    }*/
    // 将jsonVector中的内容添加到JsonArray中
    for (const auto& jsonStr : text) {
        DynamicJsonDocument tempDoc(HISTORY_JSON_DOC_BYTES);
        DeserializationError error = deserializeJson(tempDoc, jsonStr);
        if (!error) {
            textArray.add(tempDoc.as<JsonVariant>());
        } else {
            Serial.print("反序列化失败: ");
            Serial.println(error.c_str());
        }
    }
    // 返回构建好的JSON文档
    return data;
}

DynamicJsonDocument gen_params_http(const char *model, const char *role_set)
{
    // 创建一个容量为1500字节的动态JSON文档
    DynamicJsonDocument data(1500);

    data["model"] = model;
    data["max_tokens"] = 1024;
    data["temperature"] = 0.7;
    data["presence_penalty"] = 1.5;
    data["stream"] = true;

    // 在message对象中创建一个名为text的嵌套数组
    JsonArray textArray = data.createNestedArray("messages");

    JsonObject systemMessage = textArray.createNestedObject();
    systemMessage["role"] = "system";
    systemMessage["content"] = role_set;

    // 遍历全局变量text中的每个元素，并将其添加到text数组中
    /*for (const auto &item : text)
    {
        textArray.add(item);
    }*/
    // 将jsonVector中的内容添加到JsonArray中
    for (const auto& jsonStr : text) {
        DynamicJsonDocument tempDoc(HISTORY_JSON_DOC_BYTES);
        DeserializationError error = deserializeJson(tempDoc, jsonStr);
        if (!error) {
            textArray.add(tempDoc.as<JsonVariant>());
        } else {
            Serial.print("反序列化失败: ");
            Serial.println(error.c_str());
        }
    }
    // 返回构建好的JSON文档
    return data;
}

void processResponse(int status)
{
    // 如果Answer的长度超过SEGMENT_SHORT_BYTES且音频没有播放
    if (Answer.length() >= SEGMENT_SHORT_BYTES && (audio2.isplaying == 0) && flag == 0)
    {
        if (Answer.length() >= SEGMENT_LONG_BYTES)
        {
            // 查找第一个句号的位置
            int firstPeriodIndex = Answer.indexOf("。");
            if (firstPeriodIndex == -1)
            {
                // 如果没有找到句号，则查找第一个分号的位置
                firstPeriodIndex = Answer.indexOf("；");
            }
            if (firstPeriodIndex == -1)
            {
                // 如果没有找到分号，则查找第一个问号的位置
                firstPeriodIndex = Answer.indexOf("？");
            }
            if (firstPeriodIndex == -1)
            {
                // 如果没有找到问号，则查找第一个感叹号的位置
                firstPeriodIndex = Answer.indexOf("！");
            }
            // 如果找到
            if (firstPeriodIndex != -1)
            {
                // 提取完整的句子并播放
                String subAnswer1 = Answer.substring(0, firstPeriodIndex + UTF8_CJK_BYTES);
                Serial.print("subAnswer1:");
                Serial.println(subAnswer1);

                // 将提取的句子转换为语音，文字随语音同步显示
                speakAndDisplay(subAnswer1);

                // 记录到对话历史（不再重复显示）
                getText("assistant", subAnswer1, false);
                flag = 1;

                // 更新Answer，去掉已处理的部分
                Answer = Answer.substring(firstPeriodIndex + UTF8_CJK_BYTES);
                subAnswer1.clear();
                // 设置播放开始标志
                startPlay = true;
            }
            else
            {
                // 找不到任何断句标点：强制截断首段并播放
                // （B1 修复：原实现只打印不缩减 Answer，长文本无标点时一直不发声）
                int cut = trimToUtf8Boundary(Answer, SEGMENT_LONG_BYTES);
                if (cut <= 0) cut = SEGMENT_LONG_BYTES;   // 防御：cut 不会为 0（maxBytes≥300）
                String subAnswer1 = Answer.substring(0, cut);
                Serial.print("subAnswer1(强制截断):");
                Serial.println(subAnswer1);
                speakAndDisplay(subAnswer1);
                getText("assistant", subAnswer1, false);
                flag = 1;
                Answer = Answer.substring(cut);
                subAnswer1.clear();
                startPlay = true;
            }
        }
        else
        {
            // 查找最后一个逗号的位置
            int lastCommaIndex = Answer.lastIndexOf("，");
            if (lastCommaIndex != -1)
            {
                String subAnswer1 = Answer.substring(0, lastCommaIndex + UTF8_CJK_BYTES);
                Serial.print("subAnswer1:");
                Serial.println(subAnswer1);
                speakAndDisplay(subAnswer1);
                getText("assistant", subAnswer1, false);
                flag = 1;
                Answer = Answer.substring(lastCommaIndex + UTF8_CJK_BYTES);
                subAnswer1.clear();
                startPlay = true;
            }
            else
            {
                String subAnswer1 = Answer.substring(0, Answer.length());
                Serial.print("subAnswer1:");
                Serial.println(subAnswer1);
                speakAndDisplay(subAnswer1);
                getText("assistant", subAnswer1, false);
                flag = 1;
                Answer = Answer.substring(Answer.length());
                subAnswer1.clear();
                startPlay = true;
            }
        }
        conflag = 1;
    }
    // 存储多段子音频（guard 为保险上限，正常每轮循环都会缩减 Answer，B1 修复）
    int guard = 0;
    while (Answer.length() >= SEGMENT_SHORT_BYTES && guard++ < 50)
    {
        if (Answer.length() >= SEGMENT_LONG_BYTES)
        {
            // 查找第一个句号的位置
            int firstPeriodIndex = Answer.indexOf("。");
            if (firstPeriodIndex == -1)
            {
                // 如果没有找到句号，则查找第一个分号的位置
                firstPeriodIndex = Answer.indexOf("；");
            }
            if (firstPeriodIndex == -1)
            {
                // 如果没有找到分号，则查找第一个问号的位置
                firstPeriodIndex = Answer.indexOf("？");
            }
            if (firstPeriodIndex == -1)
            {
                // 如果没有找到问号，则查找第一个感叹号的位置
                firstPeriodIndex = Answer.indexOf("！");
            }
            // 如果找到
            if (firstPeriodIndex != -1)
            {
                subAnswers.push_back(Answer.substring(0, firstPeriodIndex + UTF8_CJK_BYTES));
                Serial.print("subAnswer");
                Serial.print(subAnswers.size() + 1);
                Serial.print("：");
                Serial.println(subAnswers[subAnswers.size() - 1]);

                Answer = Answer.substring(firstPeriodIndex + UTF8_CJK_BYTES);
            }
            else
            {
                // 找不到断句标点：强制按 SEGMENT_LONG_BYTES 截断
                // （B1 修复：原实现只打印不缩减，while 条件永远成立 → 死循环 → 看门狗复位）
                int cut = trimToUtf8Boundary(Answer, SEGMENT_LONG_BYTES);
                if (cut <= 0) cut = SEGMENT_LONG_BYTES;   // 防御：cut 不会为 0（maxBytes≥300）
                subAnswers.push_back(Answer.substring(0, cut));
                Serial.print("subAnswer");
                Serial.print(subAnswers.size() + 1);
                Serial.print("：");
                Serial.println(subAnswers[subAnswers.size() - 1]);

                Answer = Answer.substring(cut);
            }
        }
        else
        {
            int lastCommaIndex = Answer.lastIndexOf("，");
            if (lastCommaIndex != -1)
            {
                subAnswers.push_back(Answer.substring(0, lastCommaIndex + UTF8_CJK_BYTES));
                Serial.print("subAnswer");
                Serial.print(subAnswers.size() + 1);
                Serial.print("：");
                Serial.println(subAnswers[subAnswers.size() - 1]);

                Answer = Answer.substring(lastCommaIndex + UTF8_CJK_BYTES);
            }
            else
            {
                subAnswers.push_back(Answer.substring(0, Answer.length()));
                Serial.print("subAnswer");
                Serial.print(subAnswers.size() + 1);
                Serial.print("：");
                Serial.println(subAnswers[subAnswers.size() - 1]);

                Answer = Answer.substring(Answer.length());
            }
        }
    }

    // 如果status为2（回复的内容接收完成），且回复的内容小于SEGMENT_SHORT_BYTES
    if (status == 2 && flag == 0)
    {
        // 播放最终转换的文本，文字随语音同步显示
        speakAndDisplay(Answer);
        // 记录最终转换的文本到历史
        getText("assistant", Answer, false);
        Answer = "";
        conflag = 1;
        startPlay = true;
    }
}

// 将回复的文本转成语音
void onMessageCallback(WebsocketsMessage message)
{
    // 创建一个静态JSON文档对象，用于存储解析后的JSON数据（容量 1024 字节，实测够用）
    StaticJsonDocument<1024> jsonDocument;

    // 解析收到的JSON数据
    DeserializationError error = deserializeJson(jsonDocument, message.data());

    // 如果解析没有错误
    if (!error)
    {
        // 从JSON数据中获取返回码
        int code = jsonDocument["header"]["code"];

        // 如果返回码不为0，表示出错
        if (code != 0)
        {
            // 输出错误信息和完整的JSON数据
            Serial.print("sth is wrong: ");
            Serial.println(code);
            Serial.println(message.data());

            // 关闭WebSocket客户端
            webSocketClient.close();
        }
        else
        {
            // 获取JSON数据中的payload部分
            JsonObject choices = jsonDocument["payload"]["choices"];

            // 获取status状态
            int status = choices["status"];

            // 获取文本内容
            const char *content = choices["text"][0]["content"];
            const char *removeSet = "\n*$"; // 定义需要移除的符号集
            // 计算新字符串的最大长度
            int length = strlen(content) + 1;
            char *cleanedContent = new char[length];
            removeChars(content, cleanedContent, removeSet);
            Serial.println(cleanedContent);

            // 将内容追加到Answer字符串中
            Answer += cleanedContent;
            // 释放分配的内存
            delete[] cleanedContent;

            processResponse(status);
        }
    }
}

// 问题发送给讯飞星火大模型
void onEventsCallback(WebsocketsEvent event, String data)
{
    // 当WebSocket连接打开时触发
    if (event == WebsocketsEvent::ConnectionOpened)
    {
        // 向串口输出提示信息
        Serial.println("Send message to server0!");

        // 生成连接参数的JSON文档
        DynamicJsonDocument jsonData = gen_params(XF_APPID, XF_SPARK_DOMAIN, ROLE_SET);

        // 将JSON文档序列化为字符串
        String jsonString;
        serializeJson(jsonData, jsonString);

        // 向串口输出生成的JSON字符串
        Serial.println(jsonString);

        // 通过WebSocket客户端发送JSON字符串到服务器
        webSocketClient.send(jsonString);
    }
    // 当WebSocket连接关闭时触发
    else if (event == WebsocketsEvent::ConnectionClosed)
    {
        // 向串口输出提示信息
        Serial.println("Connnection0 Closed");
    }
    // 当收到Ping消息时触发
    else if (event == WebsocketsEvent::GotPing)
    {
        // 向串口输出提示信息
        Serial.println("Got a Ping!");
    }
    // 当收到Pong消息时触发
    else if (event == WebsocketsEvent::GotPong)
    {
        // 向串口输出提示信息
        Serial.println("Got a Pong!");
    }
}

void ConnServer()
{
    // 向串口输出WebSocket服务器的URL
    Serial.println("url:" + url);

    // 设置WebSocket客户端的消息回调函数
    webSocketClient.onMessage(onMessageCallback);

    // 设置WebSocket客户端的事件回调函数
    webSocketClient.onEvent(onEventsCallback);

    // 开始连接WebSocket服务器
    Serial.println("Begin connect to server0......");

    // 尝试连接到WebSocket服务器
    if (webSocketClient.connect(url.c_str()))
    {
        // 如果连接成功，输出成功信息
        Serial.println("Connected to server0!");
    }
    else
    {
        // 如果连接失败，输出失败信息
        Serial.println("Failed to connect to server0!");
    }
}

bool getTimeFromServer()
{
    HTTPClient http;                // 创建HTTPClient对象
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.begin(TIME_SERVER_URL);    // 初始化HTTP连接
    const char *headerKeys[] = {"Date"};        // 定义需要收集的HTTP头字段
    http.collectHeaders(headerKeys, sizeof(headerKeys) / sizeof(headerKeys[0]));    // 设置要收集的HTTP头字段
    int httpCode = http.GET();      // 发送HTTP GET请求
    String serverDate = http.header("Date");     // 从HTTP响应头中获取Date字段
    http.end();                     // 结束HTTP连接

    // B10 修复：请求失败或缺少 Date 头时返回 false，
    // 调用方保持 urlTime=0，下次对话强制重新鉴权（原实现静默失败导致鉴权 URL 无效）
    if (httpCode != HTTP_CODE_OK || serverDate.isEmpty())
    {
        Serial.println("WARN: 获取服务器时间失败，鉴权 URL 无效");
        return false;
    }
    Date = serverDate;
    Serial.println(Date);           // 输出获取到的Date字段到串口
    return true;
}

String getUrl(String Spark_url, String host, String path, String Date)
{
    // 拼接签名原始字符串
    String signature_origin = "host: " + host + "\n";
    signature_origin += "date: " + Date + "\n";
    signature_origin += "GET " + path + " HTTP/1.1";
    // 示例：signature_origin="host: spark-api.xf-yun.com\ndate: Mon, 04 Mar 2024 19:23:20 GMT\nGET /v3.5/chat HTTP/1.1";

    // 使用 HMAC-SHA256 进行加密
    unsigned char hmac[32];                                 // 存储HMAC结果
    mbedtls_md_context_t ctx;                               // HMAC上下文
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;          // 使用SHA256哈希算法
    const size_t messageLength = signature_origin.length(); // 签名原始字符串的长度
    const size_t keyLength = strlen(XF_API_SECRET);         // 密钥的长度

    // 初始化HMAC上下文
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
    // 设置HMAC密钥
    mbedtls_md_hmac_starts(&ctx, (const unsigned char *)XF_API_SECRET, keyLength);
    // 更新HMAC上下文
    mbedtls_md_hmac_update(&ctx, (const unsigned char *)signature_origin.c_str(), messageLength);
    // 完成HMAC计算
    mbedtls_md_hmac_finish(&ctx, hmac);
    // 释放HMAC上下文
    mbedtls_md_free(&ctx);

    // 将HMAC结果进行Base64编码
    String signature_sha_base64 = base64::encode(hmac, sizeof(hmac) / sizeof(hmac[0]));

    // 替换Date字符串中的特殊字符
    Date.replace(",", "%2C");
    Date.replace(" ", "+");
    Date.replace(":", "%3A");

    // 构建Authorization原始字符串
    String authorization_origin = "api_key=\"" + String(XF_API_KEY) + "\", algorithm=\"hmac-sha256\", headers=\"host date request-line\", signature=\"" + signature_sha_base64 + "\"";

    // 将Authorization原始字符串进行Base64编码
    String authorization = base64::encode(authorization_origin);

    // 构建最终的URL
    String url = Spark_url + '?' + "authorization=" + authorization + "&date=" + Date + "&host=" + host;

    // 向串口输出生成的URL
    Serial.println(url);

    // 返回生成的URL
    return url;
}

// 移除讯飞星火回复中没用的符号
void removeChars(const char *input, char *output, const char *removeSet)
{
    int j = 0;
    for (int i = 0; input[i] != '\0'; ++i)
    {
        bool shouldRemove = false;
        for (int k = 0; removeSet[k] != '\0'; ++k)
        {
            if (input[i] == removeSet[k])
            {
                shouldRemove = true;
                break;
            }
        }
        if (!shouldRemove)
        {
            output[j++] = input[i];
        }
    }
    output[j] = '\0'; // 结束符
}

// 问题发送给豆包大模型并接受回答，然后转成语音
void doubao()
{
    HTTPClient http;
    http.setTimeout(20000);     // 设置请求超时时间
    http.begin(DOUBAO_URL);
    http.addHeader("Content-Type", "application/json");
    String token_key = String("Bearer ") + DOUBAO_API_KEY;
    http.addHeader("Authorization", token_key);

    // 向串口输出提示信息
    Serial.println("Send message to doubao!");

    // 生成连接参数的JSON文档
    DynamicJsonDocument jsonData = gen_params_http(DOUBAO_MODEL, ROLE_SET);

    // 将JSON文档序列化为字符串
    String jsonString;
    serializeJson(jsonData, jsonString);

    // 向串口输出生成的JSON字符串
    Serial.println(jsonString);
    int httpResponseCode = http.POST(jsonString);

    if (httpResponseCode == 200) {
        // 在 stream（流式调用） 模式下，基于 SSE (Server-Sent Events) 协议返回生成内容，每次返回结果为生成的部分内容片段
        WiFiClient* stream = http.getStreamPtr();   // 返回一个指向HTTP响应流的指针，通过它可以读取服务器返回的数据

        while (stream->connected()) {   // 这个循环会一直运行，直到客户端（即stream）断开连接。
            String line = stream->readStringUntil('\n');    // 从流中读取一行字符串，直到遇到换行符\n为止
            // 检查读取的行是否以data:开头。
            // 在SSE（Server-Sent Events）协议中，服务器发送的数据行通常以data:开头，这样客户端可以识别出这是实际的数据内容。
            if (line.startsWith("data:")) {
                // 如果行以data:开头，提取出data:后面的部分，并去掉首尾的空白字符。
                String data = line.substring(5);
                data.trim();
                // 输出读取的数据，不建议，因为太多了，一次才一两个字
                //Serial.print("data: ");
                //Serial.println(data);

                int status = 0;
                StaticJsonDocument<400> jsonResponse;
                // 解析收到的数据
                DeserializationError error = deserializeJson(jsonResponse, data);

                // 如果解析没有错误
                if (!error)
                {
                    // B2 修复：delta 无 content 字段时返回 nullptr，判空后再使用，防止 strlen(nullptr) 崩溃
                    const char *content = jsonResponse["choices"][0]["delta"]["content"] | nullptr;
                    if (content != nullptr && strlen(content) > 0)
                    {
                        const char *removeSet = "\n*$"; // 定义需要移除的符号集
                        // 计算新字符串的最大长度
                        int length = strlen(content) + 1;
                        char *cleanedContent = new char[length];
                        removeChars(content, cleanedContent, removeSet);
                        Serial.println(cleanedContent);

                        // 将内容追加到Answer字符串中
                        Answer += cleanedContent;
                        // 释放分配的内存
                        delete[] cleanedContent;
                    }
                    else
                    {
                        status = 2;
                        Serial.println("status: 2");
                    }

                    processResponse(status);

                    if (status == 2)
                    {
                        stream->stop();
                        break;
                    }
                }
            }
        }
        return;
    }
    else
    {
        Serial.printf("Error %i \n", httpResponseCode);
        Serial.println(http.getString());
        http.end();
        return;
    }
}

// 问题发送给通义千问大模型并接受回答，然后转成语音
void tongyi()
{
    HTTPClient http;
    http.setTimeout(20000);     // 设置请求超时时间
    http.begin(TONGYI_URL);
    String token_key = String("Bearer ") + TONGYI_API_KEY;
    http.addHeader("Authorization", token_key);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-DashScope-SSE", "enable");

    // 向串口输出提示信息
    Serial.println("Send message to tongyiqianwen!");

    // 生成连接参数的JSON文档
    DynamicJsonDocument jsonData = gen_params_http(TONGYI_MODEL, ROLE_SET);

    // 将JSON文档序列化为字符串
    String jsonString;
    serializeJson(jsonData, jsonString);

    // 向串口输出生成的JSON字符串
    Serial.println(jsonString);
    int httpResponseCode = http.POST(jsonString);

    if (httpResponseCode == 200) {
        // 在 stream（流式调用） 模式下，基于 SSE (Server-Sent Events) 协议返回生成内容，每次返回结果为生成的部分内容片段
        WiFiClient* stream = http.getStreamPtr();   // 返回一个指向HTTP响应流的指针，通过它可以读取服务器返回的数据

        while (stream->connected()) {   // 这个循环会一直运行，直到客户端（即stream）断开连接。
            String line = stream->readStringUntil('\n');    // 从流中读取一行字符串，直到遇到换行符\n为止
            // 检查读取的行是否以data:开头。
            // 在SSE（Server-Sent Events）协议中，服务器发送的数据行通常以data:开头，这样客户端可以识别出这是实际的数据内容。
            if (line.startsWith("data:")) {
                // 如果行以data:开头，提取出data:后面的部分，并去掉首尾的空白字符。
                String data = line.substring(5);
                data.trim();
                // 输出读取的数据
                //Serial.print("data: ");
                //Serial.println(data);

                int status = 0;
                StaticJsonDocument<1024> jsonResponse;
                // 解析收到的数据
                DeserializationError error = deserializeJson(jsonResponse, data);

                // 如果解析没有错误
                if (!error)
                {
                    // B2 修复：delta 无 content 字段时返回 nullptr，判空后再使用，防止 strlen(nullptr) 崩溃
                    const char *content = jsonResponse["choices"][0]["delta"]["content"] | nullptr;
                    if (content != nullptr && strlen(content) > 0)
                    {
                        const char *removeSet = "\n*$"; // 定义需要移除的符号集
                        // 计算新字符串的最大长度
                        int length = strlen(content) + 1;
                        char *cleanedContent = new char[length];
                        removeChars(content, cleanedContent, removeSet);
                        Serial.println(cleanedContent);

                        // 将内容追加到Answer字符串中
                        Answer += cleanedContent;
                        // 释放分配的内存
                        delete[] cleanedContent;
                    }

                    if (jsonResponse["choices"][0]["finish_reason"] == "stop")
                    {
                        status = 2;
                        Serial.println("status: 2");
                    }

                    processResponse(status);

                    if (status == 2)
                    {
                        stream->stop();
                        break;
                    }
                }
            }
        }
        return;
    }
    else
    {
        Serial.printf("Error %i \n", httpResponseCode);
        Serial.println(http.getString());
        http.end();
        return;
    }
}

// 问题发送给Chatgpt并接受回答，然后转成语音
void chatgpt()
{
    HTTPClient http;
    http.setTimeout(20000);     // 设置请求超时时间
    http.begin(CHATGPT_URL);
    http.addHeader("Content-Type", "application/json");
    String token_key = String("Bearer ") + CHATGPT_API_KEY;
    http.addHeader("Authorization", token_key);

    // 向串口输出提示信息
    Serial.println("Send message to chatgpt!");

    // 生成连接参数的JSON文档
    DynamicJsonDocument jsonData = gen_params_http(CHATGPT_MODEL, ROLE_SET);

    // 将JSON文档序列化为字符串
    String jsonString;
    serializeJson(jsonData, jsonString);

    // 向串口输出生成的JSON字符串
    Serial.println(jsonString);
    int httpResponseCode = http.POST(jsonString);

    if (httpResponseCode == 200) {
        // 在 stream（流式调用） 模式下，基于 SSE (Server-Sent Events) 协议返回生成内容，每次返回结果为生成的部分内容片段
        WiFiClient* stream = http.getStreamPtr();   // 返回一个指向HTTP响应流的指针，通过它可以读取服务器返回的数据

        while (stream->connected()) {   // 这个循环会一直运行，直到客户端（即stream）断开连接。
            String line = stream->readStringUntil('\n');    // 从流中读取一行字符串，直到遇到换行符\n为止
            // 检查读取的行是否以data:开头。
            // 在SSE（Server-Sent Events）协议中，服务器发送的数据行通常以data:开头，这样客户端可以识别出这是实际的数据内容。
            if (line.startsWith("data:")) {
                // 如果行以data:开头，提取出data:后面的部分，并去掉首尾的空白字符。
                String data = line.substring(5);
                data.trim();
                // 输出读取的数据
                //Serial.print("data: ");
                //Serial.println(data);

                int status = 0;
                StaticJsonDocument<400> jsonResponse;
                // 解析收到的数据
                DeserializationError error = deserializeJson(jsonResponse, data);

                // 如果解析没有错误
                if (!error)
                {
                    if (jsonResponse["choices"][0]["finish_reason"] == "stop")
                    {
                        status = 2;
                        Serial.println("status: 2");
                    }
                    else
                    {
                        // B2 修复：delta 无 content 字段时返回 nullptr，判空后再使用，防止 strlen(nullptr) 崩溃
                        const char *content = jsonResponse["choices"][0]["delta"]["content"] | nullptr;
                        if (content != nullptr && strlen(content) > 0)
                        {
                            const char *removeSet = "\n*$"; // 定义需要移除的符号集
                            // 计算新字符串的最大长度
                            int length = strlen(content) + 1;
                            char *cleanedContent = new char[length];
                            removeChars(content, cleanedContent, removeSet);
                            Serial.println(cleanedContent);

                            // 将内容追加到Answer字符串中
                            Answer += cleanedContent;
                            // 释放分配的内存
                            delete[] cleanedContent;
                        }
                    }

                    processResponse(status);

                    if (status == 2)
                    {
                        stream->stop();
                        break;
                    }
                }
            }
        }
        return;
    }
    else
    {
        Serial.printf("Error %i \n", httpResponseCode);
        Serial.println(http.getString());
        http.end();
        return;
    }
}
