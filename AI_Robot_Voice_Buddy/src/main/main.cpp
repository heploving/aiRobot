#include "config.h"
#include "app.h"
#include "Web_Scr_set.h"
#include "recorder.h"
#include "display.h"
#include "llm.h"

int llm = 1;    // 大模型选择参数:0:豆包，1：讯飞星火，2：通义千问，3：ChatGPT

// 定义一些全局变量
bool ledstatus = true;          // 控制led闪烁
bool startPlay = false;
unsigned long urlTime = 0;
int noise = 50;                 // 噪声门限值
float lastAmbient = 0;          // 最近一次测得的环境底噪（会话间慢自适应）
int volume = 80;               // 初始音量大小（最小0，最大100）
//音乐播放
int mainStatus = 0;
int conStatus = 0;
int musicnum = 0;   //音乐位置下标
int musicplay = 0;  // 是否进入连续播放音乐状态
int cursorY = 0;
//语音唤醒
int awake_flag = 1;

// 使用动态JSON文档存储历史对话信息占用的内存过多，故改用c++中的vector向量
std::vector<String> text;

// 定义字符串变量，用于存储鉴权参数
String url = "";
String url1 = "";
String Date = "";

String askquestion = "";        //存储stt语音转文字信息，即用户的提问信息
String Answer = "";             //存储llm回答，用于语音合成（较短的回答）
std::vector<String> subAnswers; //存储llm回答，用于语音合成（较长的回答，分段存储）
int subindex = 0;               //subAnswers的下标，用于voicePlay()
String text_temp = "";          //存储超出当前屏幕的文字，在下一屏幕显示
// —— 屏幕文字与语音同步（打字机式逐字显示）——
String syncText = "";           // 当前播报中、需同步显示的文字段
int syncTotalMs = 0;            // 该段预估播报总时长（毫秒）
int syncShownBytes = 0;         // 已显示的字节数（整字符边界，只在出现新字符时重绘）
bool syncStarted = false;       // 音频是否已真正开始解码播放
unsigned long syncStartMs = 0;  // 音频真正开始的时刻
unsigned long syncReqMs = 0;    // 发起TTS请求的时刻（请求失败兜底用）
int flag = 0;           //用来确保subAnswer1一定是大模型回答最开始的内容
int conflag = 0;        //用于连续对话
int await_flag = 1;     //待机标识
int start_con = 0;      //标识是否开启了一轮对话

using namespace websockets; // 使用WebSocket命名空间
// 创建WebSocket客户端对象
WebsocketsClient webSocketClient1;  //与stt通信

// 创建音频对象
Recorder recorder;
Audio2 audio2(false, 3, I2S_NUM_1);
// 参数: 是否使用内部DAC（数模转换器）如果设置为true，将使用ESP32的内部DAC进行音频输出。否则，将使用外部I2S设备。
// 指定启用的音频通道。可以设置为1（只启用左声道）或2（只启用右声道）或3（启用左右声道）
// 指定使用哪个I2S端口。ESP32有两个I2S端口，I2S_NUM_0和I2S_NUM_1。可以根据需要选择不同的I2S端口。

// 函数声明
void speakAndDisplay(String text);
void ConnServer1();
void voicePlay();
int wifiConnect();

void voicePlay()
{
    // 检查音频是否正在播放以及回答内容是否为空
    if ((audio2.isplaying == 0) && (Answer != "" || subindex < subAnswers.size()))
    {
        if (subindex < subAnswers.size())
        {
            speakAndDisplay(subAnswers[subindex]);   // 发起TTS，文字随语音同步显示
            subindex++;
        }
        else
        {
            speakAndDisplay(Answer);
            Answer = "";
            conflag = 1;
        }
        // 设置开始播放标志
        startPlay = true;
    }
    else if (audio2.isplaying == 0 && musicplay == 1)   // 处理连续播放音乐逻辑
    {
        preferences.begin("music_store", true);
        int numMusic = preferences.getInt("numMusic", 0);
        musicnum = musicnum + 1 < numMusic ? musicnum + 1 : 0;
        
        String musicName = preferences.getString(("musicName" + String(musicnum)).c_str(), "");
        String musicID = preferences.getString(("musicId" + String(musicnum)).c_str(), "");
        Serial.println("音乐名称: " + musicName);
        Serial.println("音乐ID: " + musicID);

        String audioStreamURL = "https://music.163.com/song/media/outer/url?id=" + musicID + ".mp3";
        Serial.println(audioStreamURL.c_str());
        audio2.connecttohost(audioStreamURL.c_str());
        
        u8g2.clearBuffer();
        u8g2.sendBuffer();
        askquestion = "正在顺序播放所有音乐，当前正在播放：" + musicName;
        Serial.println(askquestion);
        // 打印内容
        displayWrappedText(askquestion.c_str(), 0, cursorY + 11, SCREEN_WIDTH);
        askquestion = "";
        preferences.end();
        startPlay = true;
    }
    else
    {
        // 如果音频正在播放或回答内容为空，不做任何操作
    }
}

void StartConversation()
{
    askquestion = "";
    Serial.printf("Start recognition\r\n\r\n");
    // 如果距离上次时间同步超过4分钟
    if (urlTime + 240000 < millis()) // 超过4分钟，重新做一次鉴权
    {
        // 更新时间戳
        urlTime = millis();
        // 从服务器获取当前时间
        getTimeFromServer();
        // 更新WebSocket连接的URL
        url = getUrl(XF_SPARK_WS, XF_SPARK_HOST, XF_SPARK_PATH, Date);
        url1 = getUrl(XF_IAT_WS, XF_IAT_HOST, XF_IAT_PATH, Date);
    }
    // 连接到WebSocket服务器1讯飞stt
    ConnServer1();
}

void setup()
{
    // 初始化串口通信，波特率为115200
    Serial.begin(115200);

    // 配置引脚模式
    // 配置按键引脚为上拉输入模式，用于boot按键检测
    pinMode(KEY_PIN, INPUT_PULLUP);

    // 将led设置为输出模式
    pinMode(LED_PIN, OUTPUT);
    // 将light设置为输出模式
    pinMode(LIGHT_PIN, OUTPUT);
    // 将light初始化为低电平
    digitalWrite(LIGHT_PIN, LOW);

    // 初始化屏幕（I2C 地址扫描、字体、开机画面）
    displayInit();

    // 初始化录音模块
    recorder.init();
    // 设置音频输出引脚和音量
    audio2.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio2.setVolume(volume);

    // 开机测量一次环境底噪，作为会话噪声门限的初值
    {
        float acc = 0;
        for (int i = 0; i < 20; i++)
        {
            recorder.Record();
            acc += calculateRMS((uint8_t *)recorder.frame(), FRAME_BYTES);
        }
        lastAmbient = acc / 20;
        noise = (int)max(lastAmbient * 1.8f, 300.0f);
        Serial.printf("NOISE init ambient=%.1f noise=%d\n", lastAmbient, noise);
    }

    // 初始化Preferences
    preferences.begin("wifi_store");
    preferences.begin("music_store");
    // 连接网络
    u8g2.setCursor(0, u8g2.getCursorY() + 12);
    u8g2.print("正在连接网络······");
    u8g2.sendBuffer();
    int result = wifiConnect();

    // 从百度服务器获取当前时间
    getTimeFromServer();
    // 使用当前时间生成WebSocket连接的URL
    url = getUrl(XF_SPARK_WS, XF_SPARK_HOST, XF_SPARK_PATH, Date);
    url1 = getUrl(XF_IAT_WS, XF_IAT_HOST, XF_IAT_PATH, Date);

    if (result == 1)
    {
        // 清空屏幕，在屏幕上输出提示信息
        u8g2.clearBuffer();
        u8g2.sendBuffer();
        u8g2.setCursor(0, 11);
        u8g2.print("网络连接成功！");
        displayWrappedText("请进行语音唤醒或按boot键开始对话！", 0, u8g2.getCursorY() + 12, SCREEN_WIDTH);
        awake_flag = 0;
    }
    else
    {
        openWeb();
    }
    // 记录当前时间，用于后续时间戳比较
    urlTime = millis();
    // 延迟1000毫秒，便于用户查看屏幕显示的信息，同时使设备充分初始化
    delay(1000);
}

void loop()
{
    // 轮询处理WebSocket客户端消息
    webSocketClient.poll();
    webSocketClient1.poll();

    // 如果有多段语音需要播放
    if (startPlay)  voicePlay();    // 调用voicePlay函数播放后续的语音

    // 音频处理循环
    audio2.loop();



    // 如果音频正在播放
    if (audio2.isplaying == 1)  digitalWrite(LED_PIN, HIGH);    // 点亮板载LED指示灯
    else    digitalWrite(LED_PIN, LOW);     // 熄灭板载LED指示灯

    // 屏幕文字与语音同步显示：检测音频真正开始后，按播报进度逐字显示
    if (syncText != "")
    {
        if (!syncStarted && audio2.isplaying && audio2.getBitRate() > 0)
        {
            // 首帧MP3解码成功（码率已知），声音即将开始
            syncStarted = true;
            syncStartMs = millis();
        }
        if (syncStarted && audio2.isplaying)
        {
            unsigned long elapsed = millis() - syncStartMs;
            int targetMs = (elapsed > (unsigned long)syncTotalMs) ? syncTotalMs : (int)elapsed;
            if (targetMs < 1)
                targetMs = 1;
            int showBytes = bytesForMs(syncText, targetMs);   // 换算成整字符边界的字节数
            if (showBytes != syncShownBytes)                  // 仅出现新字符时才重绘，避免每帧刷屏占用音频处理时间
            {
                syncShownBytes = showBytes;
                displayProgress(syncText, syncShownBytes);
            }
        }
        else if (audio2.isplaying == 0 && (syncStarted || millis() - syncReqMs > 3000))
        {
            // 播报结束（或TTS请求失败兜底）：补全显示整段文字
            displayProgress(syncText, syncText.length());
            syncText = "";
            syncStarted = false;
        }
    }
    
    // 唤醒词识别
    if (audio2.isplaying == 0 && awake_flag == 0 && await_flag == 1)
    {
        awake_flag = 1;
        StartConversation();
    }

    // 检测boot按键是否按下
    if (digitalRead(KEY_PIN) == 0)
    {
        conflag = 0;
        StartConversation();
    }
    // 连续对话
    if (audio2.isplaying == 0 && Answer == "" && subindex == subAnswers.size() && musicplay == 0 && conflag == 1)
    {
        StartConversation();
    }
}

// 发起TTS播报，文字交给同步显示（随语音逐字出现）
void speakAndDisplay(String text)
{
    // 上一段如有未显示完的内容，先补全显示，避免残留
    if (syncText != "" && syncShownBytes < syncText.length())
    {
        syncShownBytes = syncText.length();
        displayProgress(syncText, syncShownBytes);
    }
    syncText = text;
    syncTotalMs = max(1, speechMs(text));
    syncShownBytes = 0;
    syncStarted = false;
    syncReqMs = millis();
    audio2.connecttospeech(text.c_str(), "zh");
}

// 提取字符串中的数字
String extractNumber(const String &str) {
  String result;
  for (size_t i = 0; i < str.length(); i++) {
    if (isDigit(str[i])) {
      result += str[i];
    }
  }
  return result;
}

// 音量控制
void VolumeSet()
{
    String numberStr = extractNumber(askquestion);
    // 显示当前音量（声音）或者当前音量（声音）是多少
    if ((askquestion.indexOf("显示") > -1 && askquestion.indexOf("音") > -1) || (askquestion.indexOf("音") > -1 && askquestion.indexOf("多") > -1))
    {
        Serial.print("当前音量为: ");
        Serial.println(volume);
        // 在屏幕上显示音量
        showVolume(volume);
    }
    else if (numberStr.length() > 0)
    {
        volume = numberStr.toInt();
        audio2.setVolume(volume);
        Serial.print("音量已调到: ");
        Serial.println(volume);
        // 在屏幕上显示音量
        showVolume(volume);
    }
    else if (askquestion.indexOf("最") > -1 && (askquestion.indexOf("高") > -1 || askquestion.indexOf("大") > -1))
    {
        volume = 100;
        audio2.setVolume(volume);
        Serial.print("音量已调到: ");
        Serial.println(volume);
        // 在屏幕上显示音量
        showVolume(volume);
    }
    else if (askquestion.indexOf("高") > -1 || askquestion.indexOf("大") > -1)
    {
        volume += 10;
        if (volume > 100)
        {
            volume = 100;
        }
        audio2.setVolume(volume);
        Serial.print("音量已调到: ");
        Serial.println(volume);
        // 在屏幕上显示音量
        showVolume(volume);
    }
    else if (askquestion.indexOf("最") > -1 && (askquestion.indexOf("低") > -1 || askquestion.indexOf("小") > -1))
    {
        volume = 0;
        audio2.setVolume(volume);
        Serial.print("音量已调到: ");
        Serial.println(volume);
        // 在屏幕上显示音量
        showVolume(volume);
    }
    else if (askquestion.indexOf("低") > -1 || askquestion.indexOf("小") > -1)
    {
        volume -= 10;
        if (volume < 0)
        {
            volume = 0;
        }
        audio2.setVolume(volume);
        Serial.print("音量已调到: ");
        Serial.println(volume);
        // 在屏幕上显示音量
        showVolume(volume);
    }
    conflag = 1;
}
// 音乐播放处理逻辑

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
                audio2.isplaying = 0;
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
                u8g2.clearBuffer();
                u8g2.sendBuffer();
                u8g2.setCursor(0, 0);
                u8g2.print("user: ");
                displayWrappedText(askquestion.c_str(), u8g2.getCursorX(), u8g2.getCursorY() + 2, SCREEN_WIDTH);
                cursorY = u8g2.getCursorY() + 1;
                u8g2.setCursor(0, u8g2.getCursorY() + 2);

                String musicName = "";
                String musicID = "";
                preferences.begin("music_store", true);
                int numMusic = preferences.getInt("numMusic", 0);
                if (musicplay == 1)
                    musicplay = 1;

                if (askquestion.indexOf("不想") > -1 || askquestion.indexOf("暂停") > -1)
                {
                    musicplay = 0;
                    Answer = "好的，那主人还有其它吩咐吗？";
                    speakAndDisplay(Answer);
                    Answer = "";
                    conStatus = 0;
                    conflag = 1;
                }
                else if (askquestion.indexOf("上一") > -1)
                {
                    musicnum = musicnum - 1 >= 0 ? musicnum - 1 : numMusic - 1;
                    musicName = preferences.getString(("musicName" + String(musicnum)).c_str(), "");
                    musicID = preferences.getString(("musicId" + String(musicnum)).c_str(), "");
                    Serial.println("音乐名称: " + musicName);
                    Serial.println("音乐ID: " + musicID);

                    String audioStreamURL = "https://music.163.com/song/media/outer/url?id=" + musicID + ".mp3";
                    Serial.println(audioStreamURL.c_str());
                    audio2.connecttohost(audioStreamURL.c_str());
                    
                    if (musicplay == 0)
                        askquestion = "正在播放音乐：" + musicName;
                    else
                        askquestion = "正在顺序播放所有音乐，当前正在播放：" + musicName;
                    Serial.println(askquestion);
                    displayWrappedText(askquestion.c_str(), u8g2.getCursorX(), u8g2.getCursorY() + 2, SCREEN_WIDTH);
                    startPlay = true;   // 设置播放开始标志
                    if (musicplay == 0)
                    {
                        flag = 1;
                        Answer = "音乐播放完了，主人还想听什么音乐吗？";
                    }
                    conflag = 1;
                }
                else if (askquestion.indexOf("下一") > -1)
                {
                    musicnum = musicnum + 1 < numMusic ? musicnum + 1 : 0;
                    musicName = preferences.getString(("musicName" + String(musicnum)).c_str(), "");
                    musicID = preferences.getString(("musicId" + String(musicnum)).c_str(), "");
                    Serial.println("音乐名称: " + musicName);
                    Serial.println("音乐ID: " + musicID);

                    String audioStreamURL = "https://music.163.com/song/media/outer/url?id=" + musicID + ".mp3";
                    Serial.println(audioStreamURL.c_str());
                    audio2.connecttohost(audioStreamURL.c_str());
                    
                    if (musicplay == 0)
                        askquestion = "正在播放音乐：" + musicName;
                    else
                        askquestion = "正在顺序播放所有音乐，当前正在播放：" + musicName;
                    Serial.println(askquestion);
                    displayWrappedText(askquestion.c_str(), u8g2.getCursorX(), u8g2.getCursorY() + 2, SCREEN_WIDTH);
                    startPlay = true;   // 设置播放开始标志
                    if (musicplay == 0)
                    {
                        flag = 1;
                        Answer = "音乐播放完了，主人还想听什么音乐吗？喵~";
                    }
                    conflag = 1;
                }
                else if ((askquestion.indexOf("再听") > -1 || askquestion.indexOf("再放") > -1 || askquestion.indexOf("再来") > -1) && askquestion.indexOf("一") > -1)
                {
                    musicName = preferences.getString(("musicName" + String(musicnum)).c_str(), "");
                    musicID = preferences.getString(("musicId" + String(musicnum)).c_str(), "");
                    Serial.println("音乐名称: " + musicName);
                    Serial.println("音乐ID: " + musicID);

                    String audioStreamURL = "https://music.163.com/song/media/outer/url?id=" + musicID + ".mp3";
                    Serial.println(audioStreamURL.c_str());
                    audio2.connecttohost(audioStreamURL.c_str());
                    
                    if (musicplay == 0)
                        askquestion = "正在播放音乐：" + musicName;
                    else
                        askquestion = "正在顺序播放所有音乐，当前正在播放：" + musicName;
                    Serial.println(askquestion);
                    displayWrappedText(askquestion.c_str(), u8g2.getCursorX(), u8g2.getCursorY() + 2, SCREEN_WIDTH);
                    startPlay = true;   // 设置播放开始标志
                    if (musicplay == 0)
                    {
                        flag = 1;
                        Answer = "音乐播放完了，主人还想听什么音乐吗？喵~";
                    }
                    conflag = 1;
                }
                else if (askquestion.indexOf("听") > -1 || askquestion.indexOf("来") > -1 || askquestion.indexOf("放") > -1 || askquestion.indexOf("换") > -1)
                {
                    if (askquestion.indexOf("随便") > -1)
                    {
                        // 设置随机数种子
                        srand(time(NULL));
                        musicnum = rand() % numMusic;
                        musicName = preferences.getString(("musicName" + String(musicnum)).c_str(), "");
                        musicID = preferences.getString(("musicId" + String(musicnum)).c_str(), "");
                        Serial.println("音乐名称: " + musicName);
                        Serial.println("音乐ID: " + musicID);
                    }
                    else if (askquestion.indexOf("连续") > -1 || askquestion.indexOf("顺序") > -1 || askquestion.indexOf("所有") > -1)
                    {
                        musicplay = 1;
                        if (askquestion.indexOf("继续") == -1)
                            musicnum = 0;
                        musicName = preferences.getString(("musicName" + String(musicnum)).c_str(), "");
                        musicID = preferences.getString(("musicId" + String(musicnum)).c_str(), "");
                    }
                    else if (askquestion.indexOf("最喜欢的") > -1 || askquestion.indexOf("最爱的") > -1)
                    {
                        musicName = "Avid";
                        musicID = "1862822901";
                        Serial.println("音乐名称: " + musicName);
                        Serial.println("音乐ID: " + musicID);
                        for (int i = 0; i < numMusic; ++i)
                        {
                            if (preferences.getString(("musicId" + String(i)).c_str(), "") == musicID)  musicnum = i;
                        }
                    }
                    else    // 查询歌名
                    {
                        for (int i = 0; i < numMusic; ++i)
                        {
                            musicName = preferences.getString(("musicName" + String(i)).c_str(), "");
                            musicID = preferences.getString(("musicId" + String(i)).c_str(), "");
                            Serial.println("音乐名称: " + musicName);
                            Serial.println("音乐ID: " + musicID);
                            if (askquestion.indexOf(musicName.c_str()) > -1)
                            {
                                Serial.println("找到了！");
                                musicnum = i;
                                break;
                            }
                            else
                            {
                                musicID = "";
                            }
                        }
                    }

                    if (musicID == "") 
                    {
                        Serial.println("未找到对应的音乐！");
                        Answer = "主人，曲库里还没有这首歌哦，换一首吧，喵~";
                        speakAndDisplay(Answer);
                        Answer = "";
                        conflag = 1;
                    } 
                    else 
                    {
                        String audioStreamURL = "https://music.163.com/song/media/outer/url?id=" + musicID + ".mp3";
                        Serial.println(audioStreamURL.c_str());
                        audio2.connecttohost(audioStreamURL.c_str());
                        
                        if (musicplay == 0)
                            askquestion = "正在播放音乐：" + musicName;
                        else
                            askquestion = "正在顺序播放所有音乐，当前正在播放：" + musicName;
                        Serial.println(askquestion);
                        displayWrappedText(askquestion.c_str(), u8g2.getCursorX(), u8g2.getCursorY() + 2, SCREEN_WIDTH);
                        startPlay = true;   // 设置播放开始标志
                        if (musicplay == 0)
                        {
                            flag = 1;
                            Answer = "音乐播放完了，主人还想听什么音乐吗？";
                        }
                        conflag = 1;
                    }
                }
                else    // 处理一般的问答请求
                {
                    musicplay = 0;
                    conStatus = 0;
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
                preferences.end();
            }
            else if (((askquestion.indexOf("听") > -1 || askquestion.indexOf("放") > -1) && (askquestion.indexOf("歌") > -1 || askquestion.indexOf("音乐") > -1) && askquestion.indexOf("九歌") == -1) || mainStatus == 1)
            {
                u8g2.clearBuffer();
                u8g2.sendBuffer();
                u8g2.setCursor(0, 0);
                u8g2.print("user: ");
                displayWrappedText(askquestion.c_str(), u8g2.getCursorX(), u8g2.getCursorY() + 2, SCREEN_WIDTH);
                cursorY = u8g2.getCursorY() + 1;
                u8g2.setCursor(0, u8g2.getCursorY() + 2);

                String musicName = "";
                String musicID = "";
                preferences.begin("music_store", true);
                int numMusic = preferences.getInt("numMusic", 0);

                if (askquestion.indexOf("不想") > -1)
                {
                    mainStatus = 0;
                    Answer = "好的，那主人还有其它吩咐吗？";
                    speakAndDisplay(Answer);
                    Answer = "";
                    conflag = 1;
                    return;
                }

                if (askquestion.indexOf("随便") > -1)
                {
                    // 设置随机数种子
                    srand(time(NULL));
                    musicnum = rand() % numMusic;
                    musicName = preferences.getString(("musicName" + String(musicnum)).c_str(), "");
                    musicID = preferences.getString(("musicId" + String(musicnum)).c_str(), "");
                    Serial.println("音乐名称: " + musicName);
                    Serial.println("音乐ID: " + musicID);
                }
                else if (askquestion.indexOf("连续") > -1 || askquestion.indexOf("顺序") > -1 || askquestion.indexOf("所有") > -1)
                {
                    musicplay = 1;
                    if (askquestion.indexOf("继续") == -1)
                        musicnum = 0;
                    musicName = preferences.getString(("musicName" + String(musicnum)).c_str(), "");
                    musicID = preferences.getString(("musicId" + String(musicnum)).c_str(), "");
                }
                else if (askquestion.indexOf("最喜欢的") > -1 || askquestion.indexOf("最爱的") > -1)
                {
                    musicName = "Avid";
                    musicID = "1862822901";
                    Serial.println("音乐名称: " + musicName);
                    Serial.println("音乐ID: " + musicID);
                    for (int i = 0; i < numMusic; ++i)
                    {
                        if (preferences.getString(("musicId" + String(i)).c_str(), "") == musicID)  musicnum = i;
                    }
                }
                else    // 查询歌名
                {
                    for (int i = 0; i < numMusic; ++i)
                    {
                        musicName = preferences.getString(("musicName" + String(i)).c_str(), "");
                        musicID = preferences.getString(("musicId" + String(i)).c_str(), "");
                        Serial.println("音乐名称: " + musicName);
                        Serial.println("音乐ID: " + musicID);
                        if (askquestion.indexOf(musicName.c_str()) > -1)
                        {
                            Serial.println("找到了！");
                            musicnum = i;
                            break;
                        }
                        else
                        {
                            musicID = "";
                        }
                    }
                }

                if (musicID == "") 
                {
                    mainStatus = 1;
                    Serial.println("未找到对应的音乐！");
                    Answer = "好的，主人，你想听哪首歌呢?";
                    speakAndDisplay(Answer);
                    Answer = "";
                    conflag = 1;
                } 
                else 
                {
                    mainStatus = 0;
                    // 自建音乐服务器（这里白嫖了网易云的音乐服务器），按照音乐数字id查找对应歌曲
                    String audioStreamURL = "https://music.163.com/song/media/outer/url?id=" + musicID + ".mp3";
                    Serial.println(audioStreamURL.c_str());
                    audio2.connecttohost(audioStreamURL.c_str());

                    if (musicplay == 0)
                        askquestion = "正在播放音乐：" + musicName;
                    else
                        askquestion = "开始顺序播放所有音乐，当前正在播放：" + musicName;
                    Serial.println(askquestion);
                    displayWrappedText(askquestion.c_str(), u8g2.getCursorX(), u8g2.getCursorY() + 2, SCREEN_WIDTH);
                    startPlay = true;   // 设置播放开始标志
                    conStatus = 1;
                    if (musicplay == 0)
                    {
                        flag = 1;
                        Answer = "音乐播放完了，主人还想听什么音乐吗？";
                    }
                    conflag = 1;
                }
                preferences.end();
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
            noise = (int)max(lastAmbient * 1.8f, 300.0f);
        Serial.printf("NOISE noise=%d\n", noise);
        // 无限循环，用于录制和发送音频数据
        while (1)
        {
            // 待机状态（语音唤醒状态）也可通过boot键启动
            if (digitalRead(KEY_PIN) == 0 && await_flag == 1)
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
            if (null_voice < 20 && rms > 1000) // 抑制录音初期奇奇怪怪的噪声
            {
                rms = 8.6;
            }
            printf("%d %f\n", 0, rms);

            // 跟踪环境底噪（仅取低于门限的帧，会话间慢自适应）
            frames++;
            if (frames > 20 && rms < noise && (rms < lastAmbient || lastAmbient == 0))
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

            if(null_voice >= 250)   // 10秒静音超时（16kHz下每帧40ms），给用户更多开口时间
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
                if (voice >= 8)
                {
                    voicebegin = 1;
                }
                else
                {
                    voicebegin = 0;
                }
                silence = 0;
            }

            // 如果静音达到8个周期，发送结束标志的音频数据
            if (silence == 16)
            {
                data["status"] = 2;
                data["format"] = "audio/L16;rate=16000";
                data["audio"] = base64::encode((byte *)recorder.frame(), FRAME_BYTES);
                data["encoding"] = "raw";

                String jsonString;
                serializeJson(doc, jsonString);

                webSocketClient1.send(jsonString);
                delay(40);
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
                delay(40);
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
                delay(40);
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

int wifiConnect()
{
    // 断开当前WiFi连接
    WiFi.disconnect(true);

    preferences.begin("wifi_store");
    int numNetworks = preferences.getInt("numNetworks", 0);
    if (numNetworks == 0)
    {
        // 在屏幕上输出提示信息
        u8g2.setCursor(0, u8g2.getCursorY() + 12);
        u8g2.print("无任何wifi存储信息！");
        displayWrappedText("请连接热点ESP32-Setup密码为12345678，然后在浏览器中打开http://192.168.4.1添加新的网络！", 0, u8g2.getCursorY() + 12, SCREEN_WIDTH);
        preferences.end();
        return 0;
    }

    // 获取存储的 WiFi 配置
    for (int i = 0; i < numNetworks; ++i)
    {
        String ssid = preferences.getString(("ssid" + String(i)).c_str(), "");
        String password = preferences.getString(("password" + String(i)).c_str(), "");

        // 尝试连接到存储的 WiFi 网络
        if (ssid.length() > 0 && password.length() > 0)
        {
            Serial.print("Connecting to ");
            Serial.println(ssid);
            Serial.print("password:");
            Serial.println(password);
            // 在屏幕上显示每个网络的连接情况
            u8g2.setCursor(0, u8g2.getCursorY()+12);
            u8g2.print(ssid);

            uint8_t count = 0;
            WiFi.begin(ssid.c_str(), password.c_str());
            // 等待WiFi连接成功
            while (WiFi.status() != WL_CONNECTED)
            {
                // 闪烁板载LED以指示连接状态
                digitalWrite(LED_PIN, ledstatus);
                ledstatus = !ledstatus;
                count++;

                // 如果尝试连接超过30次，则认为连接失败
                if (count >= 30)
                {
                    Serial.printf("\r\n-- wifi connect fail! --\r\n");
                    // 在屏幕上显示连接失败信息
                    u8g2.setCursor(u8g2.getCursorX()+6, u8g2.getCursorY());
                    u8g2.print("Failed!");
                    break;
                }

                // 等待100毫秒
                vTaskDelay(100);
            }

            if (WiFi.status() == WL_CONNECTED)
            {
                // 向串口输出连接成功信息和IP地址
                Serial.printf("\r\n-- wifi connect success! --\r\n");
                Serial.print("IP address: ");
                Serial.println(WiFi.localIP());

                // 输出当前空闲堆内存大小
                Serial.println("Free Heap: " + String(ESP.getFreeHeap()));
                // 在屏幕上显示连接成功信息
                u8g2.setCursor(u8g2.getCursorX()+6, u8g2.getCursorY());
                u8g2.print("Connected!");
                preferences.end();
                return 1;
            }
        }
    }
    // 清空屏幕
    u8g2.clearBuffer();
    u8g2.sendBuffer();
    // 在屏幕上输出提示信息
    u8g2.setCursor(0, 11);
    u8g2.print("网络连接失败！请检查");
    u8g2.setCursor(0, u8g2.getCursorY() + 12);
    u8g2.print("网络设备，确认可用后");
    u8g2.setCursor(0, u8g2.getCursorY() + 12);
    u8g2.print("重启设备以建立连接！");
    displayWrappedText("或者连接热点ESP32-Setup密码为12345678，然后在浏览器中打开http://192.168.4.1添加新的网络！", 0, u8g2.getCursorY() + 12, SCREEN_WIDTH);
    preferences.end();
    return 0;
}

