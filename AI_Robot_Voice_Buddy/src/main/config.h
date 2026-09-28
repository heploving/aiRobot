/**
 * @file    config.h
 * @brief   全局配置头文件：硬件引脚、大模型凭据、角色设定、全部时序与业务常量
 *
 * 本文件是全项目唯一的配置点，修改任何参数只需改这里。
 *
 * 注意：
 * - 所有大模型凭据在下方对应位置填写（讯飞必填，豆包/通义/ChatGPT 可选）
 * - 标记【时序敏感】的常量与实机验证过的语音链路相关（16kHz/40ms 帧/8 倍增益），
 *   不要随意更改，改动会直接影响唤醒率与识别准确率。
 */
#pragma once

// ==================== 硬件引脚（以实机接线为准） ====================
#define KEY_PIN 0      // boot 按键引脚（上拉输入）
#define LED_PIN 48     // 板载 LED 引脚（GPIO8 已被 OLED SCL 占用）
#define LIGHT_PIN 38   // 外接灯光引脚
#define OLED_SDA 41    // SSD1306 OLED I2C 数据线
#define OLED_SCL 42    // SSD1306 OLED I2C 时钟线
#define SCREEN_WIDTH 128   // OLED 宽（像素）
#define SCREEN_HEIGHT 64   // OLED 高（像素）

// 音频放大模块（MAX98357，I2S_NUM_1）引脚
#define I2S_DOUT 7   // DIN 引脚
#define I2S_BCLK 15  // BCLK 引脚
#define I2S_LRC 16   // LRC 引脚

// 麦克风（INMP441，I2S_NUM_0）引脚
#define MIC_I2S_BCLK 5  // SCK 引脚
#define MIC_I2S_LRC 4   // WS 引脚
#define MIC_I2S_DIN 6   // SD 引脚

// ==================== 大模型参数（用哪个模型就填哪个） ====================
// 豆包大模型（火山方舟）
#define DOUBAO_MODEL ""                                              // 在线推理接入点名称，必填
#define DOUBAO_API_KEY ""                                            // 火山引擎 API Key，必填
#define DOUBAO_URL "https://ark.cn-beijing.volces.com/api/v3/chat/completions"

// 通义千问大模型（阿里云百炼）
#define TONGYI_MODEL ""                                              // 模型名称，必填
#define TONGYI_API_KEY ""                                            // API Key，必填
#define TONGYI_URL "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions"

// ChatGPT（aihubmix 代理）
#define CHATGPT_MODEL ""                                             // 模型名称，必填
#define CHATGPT_API_KEY ""                                           // API Key，必填
#define CHATGPT_URL "https://aihubmix.com/v1/chat/completions"

// ==================== 讯飞服务参数（必填） ====================
#define XF_APPID "57c3792c"                              // 讯飞 App ID（32 位十六进制串）
#define XF_API_SECRET "YmZlOTk1NDhjYmFjYzk1N2I0MjRlYWUy"  // API Secret（32 位 base64 形串，注意与 APIKey 勿填反）
#define XF_API_KEY "65a56f0fb36ded9cdb86817db855fbe0"    // API Key

// 星火大模型 WebSocket（generalv3.5 响应更快；追求质量可换 4.0Ultra）
#define XF_SPARK_DOMAIN "generalv3.5"
#define XF_SPARK_WS "ws://spark-api.xf-yun.com/v3.5/chat"
#define XF_SPARK_HOST "spark-api.xf-yun.com"
#define XF_SPARK_PATH "/v3.5/chat"

// 讯飞语音听写（IAT）WebSocket
#define XF_IAT_WS "ws://iat-api.xfyun.cn/v2/iat"
#define XF_IAT_HOST "iat-api.xfyun.cn"
#define XF_IAT_PATH "/v2/iat"

// 讯飞 STT 语种：zh_cn 中文（支持简单英文识别），en_us 英文
#define STT_LANGUAGE "zh_cn"

// ==================== 角色设定 ====================
#define ROLE_SET "你是一个温柔的小女生，你的名字叫小白，你的性格可爱活泼，说话简短，同时温柔可爱有礼貌。"

// ==================== 语音链路时序常量【时序敏感】 ====================
#define SAMPLE_RATE_HZ 16000   // 采样率 16kHz，实测提升识别灵敏度（改采样率需同步改所有帧计数常量）
#define FRAME_MS 40            // 单帧录音时长 = 1280B / (16000Hz×2B) = 40ms
#define FRAME_BYTES 1280       // 单帧录音字节数（16bit 单声道 PCM）
#define I2S_BUF_SIZE 5120      // 一次 I2S 读入的原始字节数（= FRAME_BYTES × 4：32bit×双声道，约 40ms）
#define MIC_GAIN 8             // 麦克风数字增益倍数（信号偏弱，实测大声说话峰值仅 ~500）
#define MIC_READ_TIMEOUT_MS 100 // I2S 读超时：正常 40ms 满帧永不触发，仅硬件异常时防死等看门狗复位

// 录音噪声门限
#define NOISE_RATIO 1.8f           // 环境底噪放大系数
#define NOISE_FLOOR 300.0f         // 噪声门限下限
#define NOISE_RMS_SUPPRESS 1000    // 录音初期噪声抑制 RMS 阈值（低于此值按 8.6 处理）
#define AMBIENT_SAMPLE_COUNT 20    // 开机底噪校准采样帧数
#define AMBIENT_TRACK_START_FRAME 20 // 录音前 20 帧为初期抑制窗口，之后才开始底噪跟踪
#define SILENCE_TIMEOUT_FRAMES 250 // 10 秒静音超时（250 帧 × 40ms）
#define END_SILENCE_FRAMES 16      // 静音 16 帧（640ms）发送结束标志
#define VOICE_START_FRAMES 8       // 有声起始帧数

// ==================== 对话/播放业务常量 ====================
#define SEGMENT_SHORT_BYTES 180 // 回答超过该长度开始分段播报
#define SEGMENT_LONG_BYTES 300  // 无断句标点的超长段强制截断阈值
#define HISTORY_MAX_BYTES 800   // 对话历史总字节数上限，超出成对删除最旧问答
#define HISTORY_JSON_DOC_BYTES 1024 // 单条历史 JSON 文档容量
#define LLM_MAX_TOKENS 1024     // 大模型回复最大 token 数
#define LLM_TEMPERATURE 0.7     // 大模型采样温度
#define LLM_PRESENCE_PENALTY 1.5 // 大模型存在惩罚
#define VOLUME_MAX 100          // 最大音量
#define VOLUME_STEP 10          // 音量调节步进
#define AUTH_REFRESH_MS 240000  // 讯飞鉴权 URL 有效期（4 分钟），到期重做鉴权
#define KEY_DEBOUNCE_MS 500     // boot 按键消抖节流
#define SYNC_REQ_TIMEOUT_MS 3000 // TTS 请求发起后超时兜底（补全显示）

// 网络请求超时
#define HTTP_TIMEOUT_MS 20000          // 普通 HTTP 请求超时
#define SSE_IDLE_TIMEOUT_MS 5000       // SSE 流单行读取超时
#define STREAM_HARD_TIMEOUT_MS 120000  // SSE 流总时长硬超时
#define WIFI_ATTEMPT_MAX 30            // 单个 WiFi 重试次数
#define WIFI_ATTEMPT_DELAY_MS 100      // 重试间隔
#define TIME_SERVER_URL "https://www.baidu.com" // 从响应头 Date 获取当前时间（讯飞鉴权用）

// ==================== 网易云音乐流地址 ====================
#define NETEASE_STREAM_PREFIX "https://music.163.com/song/media/outer/url?id="
#define NETEASE_STREAM_SUFFIX ".mp3"

// ==================== 屏幕显示常量 ====================
#define CHAR_MS_CJK 190     // 中文字符估算播报时长（毫秒）
#define CHAR_MS_ASCII 90    // ASCII 字符估算播报时长（毫秒）
#define PUNCT_PAUSE_MS 250  // 标点附加停顿（毫秒）
#define CHAR_W_CJK 12       // 中文字符像素宽（12px 字体）
#define CHAR_W_ASCII 6      // ASCII 字符像素宽
#define UTF8_CJK_BYTES 3    // 中文字符 UTF-8 编码字节数
#define VOLUME_DISP_X 88    // 音量显示横坐标
