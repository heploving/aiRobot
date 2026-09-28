#ifndef WIFI_H
#define WIFI_H

/**
 * @brief 网络模块：遍历 NVS 中存储的 WiFi 并尝试连接
 *
 * WiFi 信息存于 NVS "wifi_store" 命名空间（ssid0..n/password0..n/numNetworks）。
 * 返回 1 表示连接成功，0 表示失败（无存储或全部失败）。
 */
int wifiConnect();

#endif // WIFI_H
