#ifndef WEBSETUP_H
#define WEBSETUP_H

#include <ESPAsyncWebServer.h>

/**
 * @brief AP 配网模块：断网时启动热点 ESP32-Setup 并架设配置网页
 *
 * 通过网页管理 NVS 中的 WiFi（wifi_store）与音乐（music_store）信息。
 *
 * 已知隐患（暂不修复）：AsyncWebServer 的 handler 运行在独立 task，
 * 与 loop task 并发访问 preferences/u8g2（NVS 非线程安全），
 * 实测未出问题，行为优先保持现状。
 */
extern AsyncWebServer server;    // 定义于 websetup.cpp
extern const char *ap_ssid;      // AP 热点 SSID
extern const char *ap_password;  // AP 热点密码

// 启动 AP 热点并架设配置网页（幂等，重复调用无副作用）
void openWeb();

#endif // WEBSETUP_H
