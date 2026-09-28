#include "config.h"
#include "app.h"
#include "websetup.h"
#include "recorder.h"
#include "display.h"
#include "llm.h"
#include "stt.h"
#include "commands.h"
#include "music.h"
#include "wifi.h"

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

// 应用级 NVS 存储对象（wifi_store/music_store，各模块通过 app.h 的 extern 使用）
Preferences preferences;

// 创建音频对象
Recorder recorder;
Audio2 audio2(false, 3, I2S_NUM_1);
// 参数: 是否使用内部DAC（数模转换器）如果设置为true，将使用ESP32的内部DAC进行音频输出。否则，将使用外部I2S设备。
// 指定启用的音频通道。可以设置为1（只启用左声道）或2（只启用右声道）或3（启用左右声道）
// 指定使用哪个I2S端口。ESP32有两个I2S端口，I2S_NUM_0和I2S_NUM_1。可以根据需要选择不同的I2S端口。

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
        musicPlayNext();
    }
    else
    {
        // 如果音频正在播放或回答内容为空，不做任何操作
    }
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
        for (int i = 0; i < AMBIENT_SAMPLE_COUNT; i++)
        {
            recorder.Record();
            acc += calculateRMS((uint8_t *)recorder.frame(), FRAME_BYTES);
        }
        lastAmbient = acc / AMBIENT_SAMPLE_COUNT;
        noise = (int)max(lastAmbient * NOISE_RATIO, NOISE_FLOOR);
        Serial.printf("NOISE init ambient=%.1f noise=%d\n", lastAmbient, noise);
    }

    // 连接网络
    // （B12 修复：删除两行无意义的 preferences.begin——setup 全程未使用，
    //   真正的 begin/end 在 wifiConnect/配网页 handler/音乐模块各自配对）
    u8g2.setCursor(0, u8g2.getCursorY() + 12);
    u8g2.print("正在连接网络······");
    u8g2.sendBuffer();
    int result = wifiConnect();

    // 从百度服务器获取当前时间并生成鉴权 URL
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
        else if (audio2.isplaying == 0 && (syncStarted || millis() - syncReqMs > SYNC_REQ_TIMEOUT_MS))
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
        // 连接失败时重置标志，下一轮 loop 重试，
        // 避免一次网络抖动让唤醒永久失效（connect() 同步完成，available() 即时准确）
        if (!webSocketClient1.available())
            awake_flag = 0;
    }

    // 检测boot按键是否按下（B11 修复：keyPressed 带 500ms 消抖，长按/抖动不再反复触发）
    if (keyPressed())
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

// boot 键消抖检测：500ms 节流（B11 修复，原实现长按/抖动会每轮 loop 反复触发）
bool keyPressed()
{
    static unsigned long lastTriggerMs = 0;
    if (digitalRead(KEY_PIN) != LOW) return false;
    unsigned long now = millis();
    if (now - lastTriggerMs < KEY_DEBOUNCE_MS) return false;
    lastTriggerMs = now;
    return true;
}

// 打断当前播放
// （B7 修复：原代码直接写 audio2.isplaying=0，可能造成库内部解码状态失同步。
//   必须同时调用 stopSong() 和清零 isplaying——stopSong 只清 m_f_running，
//   loop() 因 m_f_running 提前返回永远不会补清 isplaying，只调 stopSong 会
//   导致 isplaying 永久卡 1、后续播放全部失效）
void stopPlayback()
{
    audio2.stopSong();
    audio2.isplaying = 0;
}


