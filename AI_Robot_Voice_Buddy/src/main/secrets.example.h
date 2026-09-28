/**
 * @file    secrets.example.h
 * @brief   私密凭据模板（本文件入库，实际凭据文件 secrets.h 不入库）
 *
 * 使用方法：复制本文件为 src/main/secrets.h，把下面的占位符替换为
 * 你的真实凭据。secrets.h 已被 .gitignore 忽略，切勿提交到仓库。
 */
#pragma once

// ==================== 讯飞服务参数（必填） ====================
#define XF_APPID ""                                              // 讯飞 App ID（32 位十六进制串）
#define XF_API_SECRET ""                                         // API Secret（32 位 base64 形串，注意与 APIKey 勿填反）
#define XF_API_KEY ""                                            // API Key

// ==================== 大模型参数（用哪个模型就填哪个） ====================
// 豆包大模型（火山方舟）
#define DOUBAO_MODEL ""                                          // 在线推理接入点名称，必填
#define DOUBAO_API_KEY ""                                        // 火山引擎 API Key，必填

// 通义千问大模型（阿里云百炼）
#define TONGYI_MODEL ""                                          // 模型名称，必填
#define TONGYI_API_KEY ""                                        // API Key，必填

// ChatGPT（aihubmix 代理）
#define CHATGPT_MODEL ""                                         // 模型名称，必填
#define CHATGPT_API_KEY ""                                       // API Key，必填
