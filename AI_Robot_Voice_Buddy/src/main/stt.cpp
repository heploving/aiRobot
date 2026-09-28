/**
 * @file    stt.cpp
 * @brief   讯飞语音听写（IAT）模块：录音上传 + 识别结果处理 + 全部意图分发
 *
 * onMessageCallback1/onEventsCallback1 为自由函数回调（ArduinoWebsockets std::function）。
 * 录音循环在 onEventsCallback1 内阻塞执行。
 *
 * 时序约定（实机验证过，不得更改）：
 *   - 采样率 16kHz、每帧 40ms（FRAME_MS）、帧字节 1280（FRAME_BYTES）
 *   - 静音 16 帧（640ms）发送结束标志；250 帧（10 秒）静音超时退下
 *   - 识别依赖讯飞动态修正 wpgs（dwa），每帧携带完整修正文本
 */
#include "stt.h"
#include "config.h"
#include "app.h"
#include "display.h"
#include "llm.h"
#include "music.h"
#include "commands.h"
#include "recorder.h"
#include "websetup.h"
#include "base64.h"
#include <ArduinoJson.h>
#include <WiFi.h>

using namespace websockets;

// 与 STT 通信的 WebSocket 客户端
WebsocketsClient webSocketClient1;

void StartConversation()
{
    askquestion = "";
    Serial.printf("Start recognition\r\n\r\n");
    // 如果距离上次鉴权超过4分钟（差值比较，防 millis 回绕）；
    // url 为空表示开机取时间失败（如配网后重启时路由器未就绪），立即重新鉴权，
    // 否则首次唤醒会因鉴权 URL 无效而静默失效
    if (url.isEmpty() || (unsigned long)(millis() - urlTime) >= AUTH_REFRESH_MS)
    {
        // 从服务器获取当前时间并更新鉴权 URL
        // （B10 修复：失败时保持 urlTime=0，下次对话强制重新鉴权）
        if (getTimeFromServer())
        {
            url = getUrl(XF_SPARK_WS, XF_SPARK_HOST, XF_SPARK_PATH, Date);
            url1 = getUrl(XF_IAT_WS, XF_IAT_HOST, XF_IAT_PATH, Date);
            urlTime = millis();
        }
        else
        {
            urlTime = 0;
        }
    }
    // 连接到WebSocket服务器1讯飞stt
    ConnServer1();
}

void response()
{
    speakAndDisplay(Answer);   // 文字随语音同步显示
    Answer = "";
}

// 接收stt返回的语音识别文本并做相应的逻辑处理
void onMessageCallback1(WebsocketsMessage message)
{
    // 创建一个动态JSON文档对象，用于存储解析后的JSON数据，最大容量为4096字节
    DynamicJsonDocument jsonDocument(4096);

    // 解析收到的JSON数据
    DeserializationError error = deserializeJson(jsonDocument, message.data());

    if (error)
    {
        // 如果解析出错，输出错误信息和收到的消息数据
        Serial.println("error:");
        Serial.println(error.c_str());
        Serial.println(message.data());
        return;
    }
    // 如果解析没有错误，从JSON数据中获取返回码，如果返回码不为0，表示出错
    if (jsonDocument["code"] != 0)
    {
        // 输出完整的JSON数据
        Serial.println(message.data());
        // 关闭WebSocket客户端
        webSocketClient1.close();
    }
    else
    {
        // 输出收到的讯飞云返回消息
        Serial.println("xunfeiyun stt return message:");
        Serial.println(message.data());

        // 获取JSON数据中的结果部分，并提取文本内容
        JsonArray ws = jsonDocument["data"]["result"]["ws"].as<JsonArray>();

        if (jsonDocument["data"]["status"] != 2)    //处理流式返回的内容，讯飞stt最后一次会返回一个标点符号，需要和前一次返回结果拼接起来
        {
            askquestion = "";
        }

        for (JsonVariant i : ws)
        {
            for (JsonVariant w : i["cw"].as<JsonArray>())
            {
                askquestion += w["w"].as<String>();
            }
        }

        // 输出提取的问句
        Serial.println(askquestion);

        // 获取状态码，等于2表示文本已经转换完成
        if (jsonDocument["data"]["status"] == 2)
        {
            // 如果状态码为2，表示消息处理完成
            Serial.println("status == 2");
            webSocketClient1.close();

            // 如果是调声音大小还有开关灯的指令，就不打断当前的语音
            if ((askquestion.indexOf("声音") == -1 && askquestion.indexOf("音量") == -1) && !((askquestion.indexOf("开") > -1 || askquestion.indexOf("关") > -1) && askquestion.indexOf("灯") > -1) && !(askquestion.indexOf("暂停") > -1 || askquestion.indexOf("恢复") > -1))
            {
                webSocketClient.close();    //关闭llm服务器，打断上一次提问的回答生成
                stopPlayback();             // B7 修复：封装 stopSong()+isplaying=0，原直接写成员易失同步
                startPlay = false;
                Answer = "";
                flag = 0;
                subindex = 0;
                subAnswers.clear();
                text_temp = "";
            }

            // 如果正处于待机状态，则判断唤醒词是否正确
            if (await_flag == 1)
            {
                // 增加足够多的同音字可以提高唤醒率，支持多唤醒词唤醒(askquestion.indexOf("你好") > -1 || askquestion.indexOf("您好") > -1) &&
                if( (askquestion.indexOf("小白") > -1 || askquestion.indexOf("小花") > -1))
                {
                    await_flag = 0;     //退出待机状态
                    start_con = 1;      //对话开始标识
                    Answer = "我来了，主人。";
                    response();     //屏幕显示Answer以及语音播放
                    conflag = 1;
                    return;
                }
                else
                {
                    // 将awake_flag置为0，继续进行唤醒词识别
                    awake_flag = 0;
                    return;
                }
            }

            // 如果问句为空，播放错误提示语音
            if (askquestion == "")
            {
                Answer = "主人，我没有听清，请再说一遍吧";
                response();     //屏幕显示Answer以及语音播放
                conflag = 1;
            }
            else if (askquestion.indexOf("退下") > -1 || askquestion.indexOf("再见") > -1 || askquestion.indexOf("拜拜") > -1)
            {
                start_con = 0;      // 标识一轮对话结束
                musicplay = 0;
                Answer = "主人，我先退下了，有事再叫我。";
                response();     //屏幕显示Answer以及语音播放
                await_flag = 1;     // 进入待机状态
                awake_flag = 0;     // 继续进行唤醒词识别
            }
            else if (askquestion.indexOf("断开") > -1 && (askquestion.indexOf("网络") > -1 || askquestion.indexOf("连接") > -1))
            {
                // 断开当前WiFi连接
                WiFi.disconnect(true);
                u8g2.clearBuffer();
                u8g2.sendBuffer();
                u8g2.setCursor(0, 0);
                displayWrappedText("网络连接已断开，请重启设备以再次建立连接！", u8g2.getCursorX(), u8g2.getCursorY() + 2, SCREEN_WIDTH);
                openWeb();
                displayWrappedText("热点ESP32-Setup已开启，密码为12345678，可在浏览器中打开http://192.168.4.1进行网络和音乐信息配置！", 0, u8g2.getCursorY() + 12, SCREEN_WIDTH);
            }
            else if (audio2.isplaying == 1 && askquestion.indexOf("暂停") > -1)
            {
                if(audio2.isRunning())
                {
                    Serial.println("已经暂停！");
                    audio2.pauseResume();
                }
                else
                {
                    Serial.println("当前没有音频正在播放！");
                }
            }
            else if (audio2.isplaying == 1 && askquestion.indexOf("恢复") > -1)
            {
                if(!audio2.isRunning())
                {
                    Serial.println("已经恢复！");
                    audio2.pauseResume();
                }
                else
                {
                    Serial.println("当前没有音频正在暂停！");
                }
            }
            else if (askquestion.indexOf("声音") > -1 || askquestion.indexOf("音量") > -1)
            {
                VolumeSet();    //  调整音量
            }
            else if (askquestion.indexOf("开") > -1 && askquestion.indexOf("灯") > -1)
            {
                digitalWrite(LIGHT_PIN, HIGH);
                conflag = 1;
            }
            else if (askquestion.indexOf("关") > -1 && askquestion.indexOf("灯") > -1)
            {
                digitalWrite(LIGHT_PIN, LOW);
                conflag = 1;
            }
            else if (askquestion.indexOf("换") > -1 && askquestion.indexOf("模型") > -1)
            {
                String numberStr = extractNumber(askquestion);
                if (numberStr.length() > 0)
                {
                    llm = numberStr.toInt() - 1;
                    Answer = "已为你切换为第"+ numberStr + "个模型";
                }
                if (askquestion.indexOf("字节") > -1 || askquestion.indexOf("豆包") > -1)
                {
                    llm = 0;
                    Answer = "已为你切换为豆包大模型";
                }
                if (askquestion.indexOf("讯飞") > -1 || askquestion.indexOf("星火") > -1)
                {
                    llm = 1;
                    Answer = "已为你切换为星火大模型";
                }
                if (askquestion.indexOf("阿里") > -1 || askquestion.indexOf("通义") > -1 || askquestion.indexOf("千问") > -1)
                {
                    llm = 2;
                    Answer = "已为你切换为通义千问大模型";
                }
                if (askquestion.indexOf("Chat") > -1 || askquestion.indexOf("Gpt") > -1 || askquestion.indexOf("chat") > -1 || askquestion.indexOf("gpt") > -1)
                {
                    llm = 3;
                    Answer = "已为你切换为Chatgpt大模型";
                }
                response();     //屏幕显示Answer以及语音播放
                conflag = 1;
            }
            else if (conStatus == 1)
            {
                // 连续播放音乐状态下的指令处理（含一般问答兜底）
                handleMusicInConStatus();
            }
            else if (((askquestion.indexOf("听") > -1 || askquestion.indexOf("放") > -1) && (askquestion.indexOf("歌") > -1 || askquestion.indexOf("音乐") > -1) && askquestion.indexOf("九歌") == -1) || mainStatus == 1)
            {
                // 播放音乐入口指令处理
                handleMusicEntry();
            }
            else    // 处理一般的问答请求
            {
                u8g2.clearBuffer();
                u8g2.sendBuffer();
                u8g2.setCursor(0, 0);
                getText("user", askquestion);
                if (askquestion.indexOf("天气") > -1 || askquestion.indexOf("几点了") > -1 || askquestion.indexOf("日期") > -1)
                    ConnServer();
                else
                {
                    switch (llm)
                    {
                    case 0:
                        doubao();       // 豆包
                        break;
                    case 1:
                        ConnServer();   // 讯飞星火
                        break;
                    case 2:
                        tongyi();       // 通义千问
                        break;
                    case 3:
                        chatgpt();       // chatgpt
                        break;
                    default:
                        ConnServer();   // 讯飞星火
                        break;
                    }
                }
            }
        }
    }
}

// 录音
void onEventsCallback1(WebsocketsEvent event, String data)
{
    // 当WebSocket连接打开时触发
    if (event == WebsocketsEvent::ConnectionOpened)
    {
        // 向串口输出提示信息
        Serial.println("Send message to xunfeiyun stt!");

        // 初始化变量
        int silence = 0;
        int firstframe = 1;
        int voicebegin = 0;
        int voice = 0;
        int null_voice = 0;
        int frames = 0;

        // 创建一个静态JSON文档对象，2000一般够了，不够可以再加（最多不能超过4096），但是可能会发生内存溢出
        StaticJsonDocument<2000> doc;

        if (await_flag == 1)
        {
            u8g2.clearBuffer();
            u8g2.sendBuffer();
            u8g2.setCursor(0, 11);
            u8g2.print("待机中......");
            u8g2.sendBuffer();
        }
        else if (conflag == 1)
        {
            u8g2.clearBuffer();
            u8g2.sendBuffer();
            u8g2.setCursor(0, 11);
            u8g2.print("连续对话中，请说话！");
            u8g2.sendBuffer();
        }
        else
        {
            u8g2.setCursor(0, 59);
            u8g2.print("请说话！");
            u8g2.sendBuffer();
        }
        conflag = 0;

        Serial.println("开始录音");
        // 使用最近一次测得的环境底噪更新门限（录音立即开始，无测量空窗）
        if (lastAmbient > 0)
            noise = (int)max(lastAmbient * NOISE_RATIO, NOISE_FLOOR);
        Serial.printf("NOISE noise=%d\n", noise);
        // 无限循环，用于录制和发送音频数据
        while (1)
        {
            // 待机状态（语音唤醒状态）也可通过boot键启动（B11 修复：keyPressed 带 500ms 消抖）
            if (keyPressed() && await_flag == 1)
            {
                start_con = 1;      //对话开始标识
                await_flag = 0;
                webSocketClient1.close();
                break;
            }
            // 清空JSON文档
            doc.clear();

            // 创建data对象
            JsonObject data = doc.createNestedObject("data");

            // 录制音频数据
            recorder.Record();

            // 计算音频数据的RMS值
            float rms = calculateRMS((uint8_t *)recorder.frame(), FRAME_BYTES);
            if (null_voice < AMBIENT_TRACK_START_FRAME && rms > NOISE_RMS_SUPPRESS) // 抑制录音初期奇奇怪怪的噪声
            {
                rms = 8.6;   // 实测的合理静音帧 RMS 替换值
            }
            printf("%d %f\n", 0, rms);

            // 跟踪环境底噪（仅取低于门限的帧，会话间慢自适应）
            frames++;
            if (frames > AMBIENT_TRACK_START_FRAME && rms < noise && (rms < lastAmbient || lastAmbient == 0))
                lastAmbient = rms;

            // 保持音频播放状态机运行（若录音期间有语音播放不卡顿）
            audio2.loop();

            // stt连接断开时结束本次录音，避免系统卡死在录音循环
            if (!webSocketClient1.available())
            {
                Serial.println("stt连接断开，结束本次录音");
                await_flag = 1;
                awake_flag = 0;
                webSocketClient1.close();
                return;
            }

            if(null_voice >= SILENCE_TIMEOUT_FRAMES)   // 10秒静音超时（16kHz下每帧40ms），给用户更多开口时间
            {
                if (start_con == 1)     // 表示正处于对话中，才回复退下，没有进入对话则继续待机
                {
                    start_con = 0;      // 退出对话
                    Answer = "主人，我先退下了，有事再找我~";
                    response();     //屏幕显示Answer以及语音播放
                }
                // 标识正处于待机状态
                await_flag = 1;
                // 将awake_flag置为0,继续进行唤醒词识别
                awake_flag = 0;
                // 录音超时，断开本次连接
                webSocketClient1.close();
                Serial.println("录音结束");
                return;
            }

            // 判断是否为噪音
            if (rms < noise)
            {
                null_voice++;
                if (voicebegin == 1)
                {
                    silence++;
                }
            }
            else
            {
                if (null_voice > 0)
                    null_voice--;
                voice++;
                if (voice >= VOICE_START_FRAMES)
                {
                    voicebegin = 1;
                }
                else
                {
                    voicebegin = 0;
                }
                silence = 0;
            }

            // 静音达到 16 帧（16×40ms=640ms）时发送结束标志（B14 修复：原注释误写"8个周期"）
            if (silence == END_SILENCE_FRAMES)
            {
                data["status"] = 2;
                data["format"] = "audio/L16;rate=16000";
                data["audio"] = base64::encode((byte *)recorder.frame(), FRAME_BYTES);
                data["encoding"] = "raw";

                String jsonString;
                serializeJson(doc, jsonString);

                webSocketClient1.send(jsonString);
                delay(FRAME_MS);
                Serial.println("录音结束");
                break;
            }

            // 处理第一帧音频数据
            if (firstframe == 1)
            {
                data["status"] = 0;
                data["format"] = "audio/L16;rate=16000";
                data["audio"] = base64::encode((byte *)recorder.frame(), FRAME_BYTES);
                data["encoding"] = "raw";

                JsonObject common = doc.createNestedObject("common");
                common["app_id"] = XF_APPID;

                JsonObject business = doc.createNestedObject("business");
                business["domain"] = "iat";
                business["language"] = STT_LANGUAGE;
                business["accent"] = "mandarin";
                // 不使用动态修正
                // business["vinfo"] = 1;
                // 使用动态修正
                business["dwa"] = "wpgs";
                business["vad_eos"] = 2000;

                String jsonString;
                serializeJson(doc, jsonString);

                webSocketClient1.send(jsonString);
                firstframe = 0;
                delay(FRAME_MS);
            }
            else
            {
                // 处理后续帧音频数据
                data["status"] = 1;
                data["format"] = "audio/L16;rate=16000";
                data["audio"] = base64::encode((byte *)recorder.frame(), FRAME_BYTES);
                data["encoding"] = "raw";

                String jsonString;
                serializeJson(doc, jsonString);

                webSocketClient1.send(jsonString);
                delay(FRAME_MS);
            }
        }
    }
    // 当WebSocket连接关闭时触发
    else if (event == WebsocketsEvent::ConnectionClosed)
    {
        // 向串口输出提示信息
        Serial.println("Connnection1 Closed");
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

/*---------基本不需要再改的函数---------*/
void ConnServer1()
{
    // Serial.println("url1:" + url1);
    webSocketClient1.onMessage(onMessageCallback1);
    webSocketClient1.onEvent(onEventsCallback1);
    // Connect to WebSocket
    Serial.println("Begin connect to server1......");
    if (webSocketClient1.connect(url1.c_str()))
    {
        Serial.println("Connected to server1!");
    }
    else
    {
        Serial.println("Failed to connect to server1!");
    }
}
