#ifndef MUSIC_H
#define MUSIC_H

#include <Arduino.h>

/**
 * @brief 音乐播放模块：语音指令解析 + 网易云流播放 + 顺序播放
 *
 * 音乐信息存于 NVS "music_store" 命名空间（musicName0..n/musicId0..n/numMusic）。
 * 共享状态（musicnum/musicplay/conStatus/mainStatus 等）定义于 main.cpp、声明于 app.h。
 */
// 音乐指令统一处理：连续播放模式（conStatus==1）与播放入口共用，
// 支持 不想听/上一首/下一首/再听一遍/随便/顺序/最喜欢/查歌名/一般问答
void handleMusicCommand();
// 顺序播放模式下的下一首续播（原 voicePlay 中的音乐续播段迁入）
void musicPlayNext();

#endif // MUSIC_H
