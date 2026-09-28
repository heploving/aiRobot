/**
 * @file    music.cpp
 * @brief   音乐播放模块：语音指令解析 + 网易云流播放 + 顺序播放
 *
 * 音乐信息存于 NVS "music_store" 命名空间（musicName0..n/musicId0..n/numMusic）。
 * 共享状态（musicnum/musicplay/conStatus/mainStatus 等）定义于 main.cpp、声明于 app.h。
 * 音乐流地址白嫖网易云服务器（NETEASE_STREAM_PREFIX + 数字id + ".mp3"，vip 音乐不支持）。
 */
#include "music.h"
#include "config.h"
#include "app.h"
#include "display.h"
#include "llm.h"
#include <esp_system.h>

// 连续播放音乐状态（conStatus==1）下的指令处理
void handleMusicInConStatus()
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

        String audioStreamURL = NETEASE_STREAM_PREFIX + musicID + NETEASE_STREAM_SUFFIX;
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

        String audioStreamURL = NETEASE_STREAM_PREFIX + musicID + NETEASE_STREAM_SUFFIX;
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

        String audioStreamURL = NETEASE_STREAM_PREFIX + musicID + NETEASE_STREAM_SUFFIX;
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
            // B9 修复：esp_random 硬件真随机（原 srand(time(NULL)) 无 SNTP 时间恒为 0，每次开机随机序列相同）
            if (numMusic <= 0)
                musicnum = 0;   // 防除零（原 rand()%0 未定义行为）
            else
                musicnum = (int)(esp_random() % (uint32_t)numMusic);
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
            String audioStreamURL = NETEASE_STREAM_PREFIX + musicID + NETEASE_STREAM_SUFFIX;
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

// 播放音乐入口指令处理（听歌/放歌）
void handleMusicEntry()
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
        // B9 修复：esp_random 硬件真随机（原 srand(time(NULL)) 无 SNTP 时间恒为 0，每次开机随机序列相同）
        if (numMusic <= 0)
            musicnum = 0;   // 防除零（原 rand()%0 未定义行为）
        else
            musicnum = (int)(esp_random() % (uint32_t)numMusic);
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
        String audioStreamURL = NETEASE_STREAM_PREFIX + musicID + NETEASE_STREAM_SUFFIX;
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

// 顺序播放模式下的下一首续播（voicePlay 中 musicplay==1 分支迁入）
void musicPlayNext()
{
    preferences.begin("music_store", true);
    int numMusic = preferences.getInt("numMusic", 0);
    musicnum = musicnum + 1 < numMusic ? musicnum + 1 : 0;

    String musicName = preferences.getString(("musicName" + String(musicnum)).c_str(), "");
    String musicID = preferences.getString(("musicId" + String(musicnum)).c_str(), "");
    Serial.println("音乐名称: " + musicName);
    Serial.println("音乐ID: " + musicID);

    String audioStreamURL = NETEASE_STREAM_PREFIX + musicID + NETEASE_STREAM_SUFFIX;
    Serial.println(audioStreamURL.c_str());
    audio2.connecttohost(audioStreamURL.c_str());

    u8g2.clearBuffer();
    u8g2.sendBuffer();
    askquestion = "正在顺序播放所有音乐，当前正在播放：" + musicName;
    Serial.println(askquestion);
    displayWrappedText(askquestion.c_str(), 0, cursorY + 11, SCREEN_WIDTH);
    askquestion = "";
    preferences.end();
    startPlay = true;
}
