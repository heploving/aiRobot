/**
 * @file    wifi.cpp
 * @brief   网络模块：遍历 NVS 中存储的 WiFi 并尝试连接
 *
 * WiFi 信息存于 NVS "wifi_store" 命名空间（ssid0..n/password0..n/numNetworks）。
 * 返回 1 表示连接成功，0 表示失败（无存储或全部失败）。
 */
#include "wifi.h"
#include "config.h"
#include "app.h"
#include "display.h"
#include <WiFi.h>

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
                if (count >= WIFI_ATTEMPT_MAX)
                {
                    Serial.printf("\r\n-- wifi connect fail! --\r\n");
                    // 在屏幕上显示连接失败信息
                    u8g2.setCursor(u8g2.getCursorX()+6, u8g2.getCursorY());
                    u8g2.print("Failed!");
                    break;
                }

                // 等待100毫秒
                vTaskDelay(WIFI_ATTEMPT_DELAY_MS);
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
                u8g2.print("Success!");
                preferences.end();
                return 1;
            }
        }
    }

    // 所有存储的网络都连接失败
    Serial.println("Unable to connect to any saved network.");
    preferences.end();
    return 0;
}
