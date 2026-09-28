/**
 * @file    websetup.cpp
 * @brief   AP 配网模块：断网时启动热点 ESP32-Setup 并架设配置网页
 *
 * 通过网页管理 NVS 中的 WiFi（wifi_store：ssid0..n/password0..n/numNetworks）
 * 与音乐（music_store：musicName0..n/musicId0..n/numMusic）信息。
 * 页面为 flash 常驻的 const char[]（ESP32 上 PROGMEM 为空宏，rodata 即 flash），
 * send 时隐式转 String（约 1KB 瞬时堆，可接受）。
 *
 * 已知隐患（暂不修复）：AsyncWebServer 的 handler 运行在独立 task，
 * 与 loop task 并发访问 preferences/u8g2（NVS 非线程安全），
 * 实测未出问题，行为优先保持现状。
 */
#include "websetup.h"
#include "config.h"
#include "app.h"
#include "display.h"
#include <WiFi.h>

// Web服务器对象
AsyncWebServer server(80);

// AP模式的SSID和密码
const char *ap_ssid = "ESP32-Setup";
const char *ap_password = "12345678";

// ==================== 静态页面（flash 常驻） ====================

static const char kHtmlRoot[] = R"rawliteral(
<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>ESP32 Configuration</title>
<style>body { font-family: Arial, sans-serif; text-align: center; background-color: #f0f0f0; } h1 { color: #333; } a { display: inline-block; padding: 10px 20px; margin: 10px; border: none; background-color: #333; color: white; text-decoration: none; cursor: pointer; } a:hover { background-color: #555; }</style></head>
<body><h1>ESP32 Configuration</h1><a href='/wifi'>Wi-Fi Management</a><a href='/music'>Music Management</a></body></html>
)rawliteral";

static const char kHtmlWifiManagement[] = R"rawliteral(
<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>Wi-Fi Management</title>
<style>body { font-family: Arial, sans-serif; text-align: center; background-color: #f0f0f0; } h1 { color: #333; } form { display: inline-block; margin-top: 20px; } input[type='text'], input[type='password'] { padding: 10px; margin: 10px 0; width: 200px; } input[type='submit'], input[type='button'] { padding: 10px 20px; margin: 10px 5px; border: none; background-color: #333; color: white; cursor: pointer; } input[type='submit']:hover, input[type='button']:hover { background-color: #555; }</style></head>
<body><h1>Wi-Fi Management</h1><form action='/save' method='post'><label for='ssid'>Wi-Fi SSID:</label><br><input type='text' id='ssid' name='ssid'><br><label for='password'>Password:</label><br><input type='password' id='password' name='password'><br><input type='submit' value='Save'></form><form action='/delete' method='post'><label for='ssid'>Wi-Fi SSID to Delete:</label><br><input type='text' id='ssid' name='ssid'><br><input type='submit' value='Delete'></form><a href='/list'><input type='button' value='List Wi-Fi Networks'></a><p><a href='/'>Go Back</a></p></body></html>
)rawliteral";

static const char kHtmlMusicManagement[] = R"rawliteral(
<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>Music Management</title>
<style>body { font-family: Arial, sans-serif; text-align: center; background-color: #f0f0f0; } h1 { color: #333; } form { display: inline-block; margin-top: 20px; } input[type='text'], input[type='password'] { padding: 10px; margin: 10px 0; width: 200px; } input[type='submit'], input[type='button'] { padding: 10px 20px; margin: 10px 5px; border: none; background-color: #333; color: white; cursor: pointer; } input[type='submit']:hover, input[type='button']:hover { background-color: #555; }</style></head>
<body><h1>Music Management</h1><form action='/saveMusic' method='post'><label for='musicName'>Music Name:</label><br><input type='text' id='musicName' name='musicName'><br><label for='musicId'>Music ID:</label><br><input type='text' id='musicId' name='musicId'><br><input type='submit' value='Save Music'></form><form action='/deleteMusic' method='post'><label for='musicName'>Music Name to Delete:</label><br><input type='text' id='musicName' name='musicName'><br><input type='submit' value='Delete Music'></form><a href='/listMusic'><input type='button' value='List Saved Music'></a><p><a href='/'>Go Back</a></p></body></html>
)rawliteral";

static const char kHtmlListWifiHeader[] =
    "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>ESP32 Wi-Fi Configuration</title></head><body><h1>Saved Wi-Fi Networks</h1><ul>";
static const char kHtmlListMusicHeader[] =
    "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>ESP32 Music Configuration</title></head><body><h1>Saved Music</h1><ul>";
static const char kHtmlListFooter[] =
    "</ul><p><a href='/'>Go Back</a></p></body></html>";

// ==================== 通用工具 ====================

// 统一的成功/失败响应页（原 6 处内联 HTML 的合并）
static void sendStatusPage(AsyncWebServerRequest *request, const char *title, const char *message)
{
    String html = String("<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>") +
                  title + "</title></head><body><h1>" + title + "</h1><p>" + message +
                  "</p><p><a href='/'>Go Back</a></p></body></html>";
    request->send(200, "text/html", html);
}

// 通用条目更新：nameKey/valueKey 形如 "ssid"/"password" 或 "musicName"/"musicId"
// 返回 true=更新了已存在条目，false=新增了一条
static bool upsertEntry(const char *ns, const char *nameKey, const char *valueKey,
                        const char *countKey, const String &name, const String &value)
{
    preferences.begin(ns, false);
    int num = preferences.getInt(countKey, 0);
    for (int i = 0; i < num; ++i)
    {
        if (preferences.getString((String(nameKey) + i).c_str(), "") == name)
        {
            preferences.putString((String(valueKey) + i).c_str(), value);
            preferences.end();
            return true;
        }
    }
    preferences.putString((String(nameKey) + num).c_str(), name);
    preferences.putString((String(valueKey) + num).c_str(), value);
    preferences.putInt(countKey, num + 1);
    preferences.end();
    return false;
}

// 通用条目删除（后续条目前移，末尾残留清除）
// 返回 true=找到并删除，false=不存在
static bool deleteEntry(const char *ns, const char *nameKey, const char *valueKey,
                        const char *countKey, const String &name)
{
    preferences.begin(ns, false);
    int num = preferences.getInt(countKey, 0);
    for (int i = 0; i < num; ++i)
    {
        if (preferences.getString((String(nameKey) + i).c_str(), "") == name)
        {
            // 后续条目前移一位
            for (int j = i; j < num - 1; ++j)
            {
                preferences.putString((String(nameKey) + j).c_str(),
                                      preferences.getString((String(nameKey) + (j + 1)).c_str(), ""));
                preferences.putString((String(valueKey) + j).c_str(),
                                      preferences.getString((String(valueKey) + (j + 1)).c_str(), ""));
            }
            // 删除末尾残留
            preferences.remove((String(nameKey) + (num - 1)).c_str());
            preferences.remove((String(valueKey) + (num - 1)).c_str());
            preferences.putInt(countKey, num - 1);
            preferences.end();
            return true;
        }
    }
    preferences.end();
    return false;
}

// 通用条目列表（返回 <li> 片段）
static String listEntries(const char *ns, const char *nameKey, const char *valueKey,
                          const char *countKey, const char *labelPrefix)
{
    String html;
    html.reserve(256);
    preferences.begin(ns, true);
    int num = preferences.getInt(countKey, 0);
    for (int i = 0; i < num; ++i)
    {
        html += String("<li>") + labelPrefix + i + ": " +
                preferences.getString((String(nameKey) + i).c_str(), "") + " " +
                preferences.getString((String(valueKey) + i).c_str(), "") + "</li>";
    }
    preferences.end();
    return html;
}

// ==================== 路由 handler ====================

// 处理根路径的请求
void handleRoot(AsyncWebServerRequest *request)
{
    request->send(200, "text/html", kHtmlRoot);
}

// wifi配置界面
void handleWifiManagement(AsyncWebServerRequest *request)
{
    request->send(200, "text/html", kHtmlWifiManagement);
}

// 音乐信息配置界面
void handleMusicManagement(AsyncWebServerRequest *request)
{
    request->send(200, "text/html", kHtmlMusicManagement);
}

// 添加或更新wifi信息逻辑
void handleSave(AsyncWebServerRequest *request)
{
    showConfigStatus("进入网络配置！");
    Serial.println("Start Save!");
    String ssid = request->arg("ssid");
    String password = request->arg("password");

    // 输入校验：SSID/密码非空且不超过 32 字节
    if (ssid.isEmpty() || password.isEmpty() || ssid.length() > 32 || password.length() > 32)
    {
        Serial.println("Invalid input for WiFi save!");
        showConfigStatus("进入网络配置！", "输入无效，请重试！");
        sendStatusPage(request, "Invalid Input!", "SSID and password must be 1-32 characters.");
        return;
    }

    if (upsertEntry("wifi_store", "ssid", "password", "numNetworks", ssid, password))
    {
        showConfigStatus("进入网络配置！", "wifi密码更新成功！");
        Serial.println("Success Update!");
        sendStatusPage(request, "Configuration Updated!", "Network password updated successfully.");
    }
    else
    {
        showConfigStatus("进入网络配置！", "新wifi添加成功！");
        Serial.println("Success Save!");
        sendStatusPage(request, "Configuration Saved!", "Network information added successfully.");
    }
}

// 删除wifi信息逻辑
void handleDelete(AsyncWebServerRequest *request)
{
    showConfigStatus("进入网络配置！");
    Serial.println("Start Delete!");
    String ssidToDelete = request->arg("ssid");

    if (deleteEntry("wifi_store", "ssid", "password", "numNetworks", ssidToDelete))
    {
        showConfigStatus("进入网络配置！", "wifi删除成功！");
        Serial.println("Success Delete!");
        sendStatusPage(request, "Network Deleted!", "The network has been deleted.");
    }
    else
    {
        showConfigStatus("进入网络配置！", "该wifi不存在！");
        Serial.println("Fail to Delete!");
        sendStatusPage(request, "Network Not Found!", "The specified network was not found.");
    }
}

// 显示已有wifi信息逻辑
void handleList(AsyncWebServerRequest *request)
{
    String html = kHtmlListWifiHeader;
    html += listEntries("wifi_store", "ssid", "password", "numNetworks", "ssid");
    html += kHtmlListFooter;
    request->send(200, "text/html", html);
}

// 添加或更新音乐信息逻辑
void handleSaveMusic(AsyncWebServerRequest *request)
{
    showConfigStatus("进入音乐配置！");
    Serial.println("Start Save Music!");
    String musicName = request->arg("musicName");
    String musicId = request->arg("musicId");

    // 输入校验：歌名非空，ID 非空且全为数字（网易云歌曲数字 id）
    bool idIsNumeric = !musicId.isEmpty();
    for (size_t k = 0; k < musicId.length(); ++k)
        if (!isDigit(musicId[k])) idIsNumeric = false;
    if (musicName.isEmpty() || !idIsNumeric)
    {
        Serial.println("Invalid input for music save!");
        showConfigStatus("进入音乐配置！", "输入无效，请重试！");
        sendStatusPage(request, "Invalid Input!", "Music name is required and ID must be numeric.");
        return;
    }

    if (upsertEntry("music_store", "musicName", "musicId", "numMusic", musicName, musicId))
    {
        showConfigStatus("进入音乐配置！", "音乐ID更新成功！");
        Serial.println("Success Update Music!");
        sendStatusPage(request, "Music ID Updated!", "Music ID updated successfully.");
    }
    else
    {
        showConfigStatus("进入音乐配置！", "新音乐添加成功！");
        Serial.println("Success Save Music!");
        sendStatusPage(request, "Music Saved!", "Music information added successfully.");
    }
}

// 删除音乐信息逻辑
void handleDeleteMusic(AsyncWebServerRequest *request)
{
    showConfigStatus("进入音乐配置！");
    Serial.println("Start Delete Music!");
    String musicNameToDelete = request->arg("musicName");

    if (deleteEntry("music_store", "musicName", "musicId", "numMusic", musicNameToDelete))
    {
        showConfigStatus("进入音乐配置！", "音乐删除成功！");
        Serial.println("Success Delete Music!");
        sendStatusPage(request, "Music Deleted!", "The music has been deleted.");
    }
    else
    {
        showConfigStatus("进入音乐配置！", "该音乐不存在！");
        Serial.println("Fail to Delete Music!");
        sendStatusPage(request, "Music Not Found!", "The specified music was not found.");
    }
}

// 显示已有音乐信息逻辑
void handleListMusic(AsyncWebServerRequest *request)
{
    String html = kHtmlListMusicHeader;
    html += listEntries("music_store", "musicName", "musicId", "numMusic", "musicName");
    html += kHtmlListFooter;
    request->send(200, "text/html", html);
}

void openWeb()
{
    // 幂等保护：openWeb 被调用 2 次（setup 配网失败 / 语音指令断开网络），
    // 原实现会重复注册路由并重复 softAP
    static bool webStarted = false;
    if (webStarted)
    {
        Serial.println("WebServer already started, skip");
        return;
    }
    webStarted = true;

    // 网络连接失败，启动 AP 模式创建热点用于配网和音乐信息添加
    WiFi.softAP(ap_ssid, ap_password);
    Serial.println("Started Access Point");
    // 启动 Web 服务器
    server.on("/", HTTP_GET, handleRoot);
    server.on("/wifi", HTTP_GET, handleWifiManagement);
    server.on("/music", HTTP_GET, handleMusicManagement);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/delete", HTTP_POST, handleDelete);
    server.on("/list", HTTP_GET, handleList);
    server.on("/saveMusic", HTTP_POST, handleSaveMusic);
    server.on("/deleteMusic", HTTP_POST, handleDeleteMusic);
    server.on("/listMusic", HTTP_GET, handleListMusic);

    server.begin();
    Serial.println("WebServer started, waiting for configuration...");
}
