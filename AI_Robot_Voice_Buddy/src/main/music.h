#ifndef MUSIC_H
#define MUSIC_H

#include <Arduino.h>

/**
 * @brief 音乐播放模块：语音指令解析 + 网易云流播放 + 顺序播放
 *
 * 音乐信息存于 NVS "music_store" 命名空间（musicName0..n/musicId0..n/numMusic）。
 * 共享状态（musicnum/musicplay/conStatus/mainStatus 等）定义于 main.cpp、声明于 app.h。
 */
// 连续播放音乐状态（conStatus==1）下的指令处理（原 onMessageCallback1 整块迁入）
void handleMusicInConStatus();
// 播放音乐入口指令处理（原 onMessageCallback1 的听歌/放歌分支块迁入）
void handleMusicEntry();
// 顺序播放模式下的下一首续播（原 voicePlay 中的音乐续播段迁入）
void musicPlayNext();

#endif // MUSIC_H
