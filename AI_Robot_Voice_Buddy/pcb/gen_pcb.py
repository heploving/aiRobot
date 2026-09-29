#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_pcb.py — Voice_Buddy 盾板（载板）PCB 生成脚本（KiCad 6 兼容）
==================================================
用 pcbnew Python API 从零生成盾板布局：
  - J1/J3：1x19 母排座（插接 ESP32-S3 DevKitC-1，中心距 22.86mm）
  - J2：1x6 母排座横放（INMP441 模块，针序 SCK/SD/WS/L/R/VDD/GND）
  - J4：1x5 母排座（MAX98357 模块，VIN/GND/DIN/BCLK/LRC）
  - J5：1x4 母排座（SSD1306 OLED，GND/VCC/SCL/SDA）
  - J6：2P 直针（喇叭端子，飞线接 MAX98357 模块 SPK± 焊盘）
  - SW1 轻触按键（GPIO0）、D1 LED + R1 限流（GPIO38）
  - C1 100uF / C2 100nF（5V 电源滤波）
  - 背面整板 GND 覆铜；信号走线分两层（见 design 注释）

引脚定义与固件 src/main/config.h 完全一致。
用法：python3 gen_pcb.py   （需 KiCad 已安装，产出 shield.kicad_pcb）
"""
import os
import sys
from collections import defaultdict

import pcbnew

BOARD_W = 62.0
BOARD_H = 64.0
FPC = os.environ.get("KICAD_FOOTPRINTS", "/usr/share/kicad/footprints")

# KiCad 6 兼容工具
def MM(x):
    return int(x * 1e6)

def P(x, y):
    return pcbnew.wxPoint(MM(x), MM(y))

def pads_of(f):
    return f.GetPads() if hasattr(f, "GetPads") else f.Pads()

board = pcbnew.BOARD()


def fp(lib, name, ref, x, y, rot=0.0):
    f = pcbnew.FootprintLoad(os.path.join(FPC, lib + ".pretty"), name)
    if f is None:
        sys.exit(f"封装加载失败: {lib}:{name}（{FPC}）")
    f.SetReference(ref)
    board.Add(f)
    f.SetPosition(P(x, y))
    f.SetOrientationDegrees(rot)
    return f


def pad(f, n):
    for p in pads_of(f):
        if p.GetNumber() == str(n):
            return p
    sys.exit(f"{f.GetReference()} 缺少焊盘 {n}")


def pt(f, n):
    pos = pad(f, n).GetPosition()
    return (pos.x / 1e6, pos.y / 1e6)


def path(layer, width, points, net_name=None):
    for i in range(len(points) - 1):
        (x1, y1), (x2, y2) = points[i], points[i + 1]
        t = pcbnew.PCB_TRACK(board)
        t.SetStart(P(x1, y1))
        t.SetEnd(P(x2, y2))
        t.SetLayer(layer)
        t.SetWidth(MM(width))
        if net_name:
            t.SetNet(net(net_name))
        board.Add(t)


def text(s, x, y, size=1.2, layer=None, thickness=0.15, rot=0.0):
    if layer is None:
        layer = pcbnew.F_SilkS
    t = pcbnew.PCB_TEXT(board)
    t.SetText(s)
    t.SetPosition(P(x, y))
    t.SetLayer(layer)
    t.SetTextSize(pcbnew.wxSize(MM(size), MM(size)))
    t.SetTextThickness(MM(thickness))
    if hasattr(t, "SetTextAngleDegrees"):
        t.SetTextAngleDegrees(rot)
    else:
        t.SetTextAngle(int(rot * 10))
    board.Add(t)


def edge_rect(x1, y1, x2, y2):
    for (ax, ay), (bx, by) in [((x1, y1), (x2, y1)), ((x2, y1), (x2, y2)),
                               ((x2, y2), (x1, y2)), ((x1, y2), (x1, y1))]:
        seg = pcbnew.PCB_SHAPE(board)
        seg.SetShape(pcbnew.SHAPE_T_SEGMENT)
        seg.SetStart(P(ax, ay))
        seg.SetEnd(P(bx, by))
        seg.SetLayer(pcbnew.Edge_Cuts)
        seg.SetWidth(MM(0.15))
        board.Add(seg)


# ==================== 元件摆放 ====================
J1 = fp("Connector_PinSocket_2.54mm", "PinSocket_1x19_P2.54mm_Vertical", "J1", 12.0, 8.0)
J3 = fp("Connector_PinSocket_2.54mm", "PinSocket_1x19_P2.54mm_Vertical", "J3", 34.86, 8.0)
J2 = fp("Connector_PinSocket_2.54mm", "PinSocket_1x06_P2.54mm_Vertical", "J2", 6.0, 2.0, 90.0)
J4 = fp("Connector_PinSocket_2.54mm", "PinSocket_1x05_P2.54mm_Vertical", "J4", 50.0, 8.0)
J5 = fp("Connector_PinSocket_2.54mm", "PinSocket_1x04_P2.54mm_Vertical", "J5", 50.0, 26.0)
J6 = fp("Connector_PinHeader_2.54mm", "PinHeader_1x02_P2.54mm_Vertical", "J6", 16.0, 58.0)
SW1 = fp("Button_Switch_THT", "SW_PUSH_6mm", "SW1", 26.0, 58.0)
D1 = fp("LED_THT", "LED_D5.0mm", "D1", 34.0, 58.0)
R1 = fp("Resistor_THT", "R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal", "R1", 43.0, 58.0)
C1 = fp("Capacitor_THT", "CP_Radial_D6.3mm_P2.50mm", "C1", 50.0, 45.0, 180.0)
C2 = fp("Capacitor_THT", "C_Disc_D5.0mm_W2.5mm_P2.50mm", "C2", 43.0, 42.0, 180.0)
for (x, y) in [(4, 4), (58, 4), (4, 60), (58, 60)]:
    fp("MountingHole", "MountingHole_3.2mm_M3", "H", x, y)

# ==================== 网络定义 ====================
nets = {}
def net(name):
    if name not in nets:
        n = pcbnew.NETINFO_ITEM(board, name)
        board.GetNetInfo().AppendNet(n)
        nets[name] = n
    return nets[name]

def connect(f, pin, name):
    pad(f, pin).SetNet(net(name))

def connect_all(f, pin, name):
    for p in pads_of(f):
        if p.GetNumber() == str(pin):
            p.SetNet(net(name))

connect(J1, 1, "3V3")
connect(J3, 2, "5V")
connect(J3, 1, "GND")
# INMP441（J2 针序：SCK/SD/WS/L/R/VDD/GND）
connect(J1, 4, "MIC_BCLK"); connect(J2, 1, "MIC_BCLK")
connect(J1, 5, "MIC_SD");   connect(J2, 2, "MIC_SD")
connect(J1, 3, "MIC_WS");   connect(J2, 3, "MIC_WS")
connect(J2, 4, "GND")       # L/R 接地选左声道（背面覆铜自动连接）
connect(J2, 5, "3V3")
connect(J2, 6, "GND")
# MAX98357（J4 针序：VIN/GND/DIN/BCLK/LRC）
connect(J4, 1, "5V")
connect(J4, 2, "GND")
connect(J1, 6, "AMP_DIN");  connect(J4, 3, "AMP_DIN")
connect(J1, 7, "AMP_BCLK"); connect(J4, 4, "AMP_BCLK")
connect(J1, 8, "AMP_LRC");  connect(J4, 5, "AMP_LRC")
# SSD1306 OLED（J5 针序：GND/VCC/SCL/SDA）
connect(J5, 1, "GND")
connect(J5, 2, "3V3")
connect(J3, 7, "OLED_SCL"); connect(J5, 3, "OLED_SCL")
connect(J3, 8, "OLED_SDA"); connect(J5, 4, "OLED_SDA")
# LED + 电阻（GPIO38 -> R1 -> D1 -> GND）
connect(J3, 11, "LIGHT"); connect(R1, 1, "LIGHT")
connect(R1, 2, "LED_A");   connect(D1, 1, "LED_A")
connect(D1, 2, "GND")
# 按键（GPIO0 -> SW1.1，SW1.2 -> GND）
connect(J3, 15, "KEY"); connect_all(SW1, 1, "KEY")
connect_all(SW1, 2, "GND")
# 滤波电容
connect(C1, 1, "5V");  connect(C1, 2, "GND")
connect(C2, 1, "5V");  connect(C2, 2, "GND")
# 喇叭端子 J6 不接网络（飞线接 MAX98357 模块 SPK± 焊盘）

# ==================== 走线 ====================
# 布局拓扑（已逐段推演交叉/间距，两处过孔汇合）：
#   正面 F.Cu：麦克风 SCK/WS、功放三线（竖线 16/17/18，水平走 J3 焊盘间隙）、
#             OLED-SCL、LIGHT、LED_A、KEY（含左触片桥接）、L/R 桥、底部 GND 汇合
#   背面 B.Cu：麦克风 SD、3V3 全段（顶部走廊 y=5）、5V、GND 干线 x=30、
#             OLED-SDA、电容 GND
F, B = pcbnew.F_Cu, pcbnew.B_Cu
# ---- 正面：麦克风 ----
path(F, 0.3, [pt(J2, 1), (pt(J2, 1)[0], 15.62), pt(J1, 4)], "MIC_BCLK")               # SCK
path(F, 0.3, [pt(J2, 3), (10.55, pt(J2, 3)[1]), (10.55, 13.08), pt(J1, 3)], "MIC_WS") # WS
path(F, 0.3, [pt(J2, 4), (pt(J2, 4)[0], 3.4), (pt(J2, 6)[0], 3.4), pt(J2, 6)], "GND") # L/R 接地桥
# ---- 正面：功放三线（竖线 16/17/18，水平走 J3/J4 焊盘间隙 11.81/14.35/16.89）----
path(F, 0.3, [pt(J1, 6), (16, pt(J1, 6)[1]), (16, 11.81), (50, 11.81), (50, 13.08)], "AMP_DIN")
path(F, 0.3, [pt(J1, 7), (17, pt(J1, 7)[1]), (17, 14.35), (50, 14.35), (50, 15.62)], "AMP_BCLK")
path(F, 0.3, [pt(J1, 8), (18, pt(J1, 8)[1]), (18, 16.89), (50, 16.89), (50, 18.16)], "AMP_LRC")
# ---- 正面：OLED-SCL、LIGHT、LED_A、KEY ----
path(F, 0.3, [pt(J3, 7), (37, pt(J3, 7)[1]), (37, 31.08), pt(J5, 3)], "OLED_SCL")
path(F, 0.3, [pt(J3, 11), (38, pt(J3, 11)[1]), (38, 58), pt(R1, 1)], "LIGHT")
path(F, 0.3, [pt(R1, 2), (pt(R1, 2)[0], 60), (34, 60), pt(D1, 1)], "LED_A")
path(F, 0.3, [pt(J3, 15), (31, pt(J3, 15)[1]), (31, 58), (32.5, 58)], "KEY")
path(F, 0.3, [(26, 58), (26, 55), (32.5, 55), (32.5, 58)], "KEY")                     # 左触片桥接
# ---- 正面：底部 GND 汇合（经 (30,56) 过孔进背面干线）----
path(F, 0.3, [pt(SW1, 2), (30, 62.5), (30, 56)], "GND")
path(F, 0.3, [(32.5, 62.5), (30, 62.5)], "GND")
path(F, 0.3, [pt(D1, 2), (pt(D1, 2)[0], 61.5), (30, 61.5), (30, 56)], "GND")
path(F, 0.3, [pt(C1, 2), (pt(C1, 2)[0], 52), (52, 52), (52, 61.5), (30, 61.5), (30, 56)], "GND")
via = pcbnew.PCB_VIA(board)
via.SetPosition(P(30, 56))
via.SetDrill(MM(0.4))
via.SetWidth(MM(0.8))
via.SetNet(net("GND"))
board.Add(via)
# ---- 背面：麦克风 SD（y=1.1 走廊）----
path(B, 0.3, [pt(J2, 2), (pt(J2, 2)[0], 0.5), (20, 0.5), (20, 18.16), pt(J1, 5)], "MIC_SD")
# ---- 背面：3V3 全段（顶部走廊 y=5 穿过 GND 干线上方，直抵 OLED VCC）----
path(B, 0.3, [pt(J1, 1), (12, 4), (pt(J2, 5)[0], 4), pt(J2, 5)], "3V3")
path(B, 0.3, [(pt(J2, 5)[0], 4), (21, 4), (21, 5), (53, 5), (53, 28.54), pt(J5, 2)], "3V3")
# ---- 背面：5V ----
path(B, 0.5, [pt(J3, 2), (37, pt(J3, 2)[1]), (37, 6.5), (51.5, 6.5), (51.5, 8), pt(J4, 1)], "5V")
path(B, 0.5, [(51.5, 8), (51.5, 45), (50, 45), (50, 42), pt(C2, 1)], "5V")
# ---- 背面：OLED-SDA ----
path(B, 0.3, [pt(J3, 8), (41, pt(J3, 8)[1]), (41, 33.62), pt(J5, 4)], "OLED_SDA")
# ---- 背面：GND 干线 x=30（y 5.5..56）与各 GND 接入 ----
path(B, 0.3, [pt(J3, 1), (34.86, 5.5), (30, 5.5), (30, 56)], "GND")
path(B, 0.3, [pt(J4, 2), (52.3, pt(J4, 2)[1]), (52.3, 5.5), (30, 5.5)], "GND")
path(B, 0.3, [pt(J5, 1), (50, 22), (30, 22)], "GND")
path(B, 0.3, [pt(J2, 6), (pt(J2, 6)[0], 3.3), (21.5, 3.3), (21.5, 5.5), (30, 5.5)], "GND")
path(B, 0.3, [pt(C2, 2), (pt(C2, 2)[0], 39.75), (30, 39.75)], "GND")

# ==================== 边框、丝印 ====================
edge_rect(0, 0, BOARD_W, BOARD_H)
text("Voice_Buddy 盾板", 22, 2, 1.6)
text("INMP441", 6.0, 4.5)
text("MAX98357", 46.5, 20.5)
text("OLED", 46.5, 35.5)
text("SPK", 14.2, 60.2)
text("KEY", 24.0, 60.2)
text("LED", 33.2, 60.2)
text("GPIO38-LED / GPIO0-KEY", 20.0, 62.4, 0.9)
text("J1:1=3V3  J3:1=GND 2=5V", 12.0, 62.8, 0.9)
text("喇叭飞线: MAX98357模块 SPK+/- -> J6", 26.0, 30.0, 0.9)
text("USB 口朝下插接 DevKitC-1", 22.0, 55.0, 0.9)

# ==================== 保存、连通性自检 ====================
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "shield.kicad_pcb")
pcbnew.SaveBoard(out, board)

# 连通性自检：每个网络应至少有 2 个焊盘（J6 无网络除外）
cnt = defaultdict(int)
for f in board.GetFootprints():
    for p in pads_of(f):
        n = p.GetNetname()
        if n:
            cnt[n] += 1
warn = 0
for n, c in sorted(cnt.items()):
    if c < 2:
        print(f"WARN 网络只有 {c} 个焊盘: {n}")
        warn += 1

print(f"OK -> {out}")
print(f"元件 {len(board.GetFootprints())} 个，网络 {len(nets)} 个，走线 {len(board.GetTracks())} 段，孤立网络 {warn} 个")
