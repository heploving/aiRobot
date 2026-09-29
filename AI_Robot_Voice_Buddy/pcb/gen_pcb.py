#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_pcb.py — Voice_Buddy 盾板（载板）PCB 生成脚本（KiCad 10 版）
==================================================
用 pcbnew Python API 从零生成盾板布局：
  - J1/J3：1x19 母排座（插接 ESP32-S3 DevKitC-1，中心距 22.86mm）
  - J2：1x6 母排座横放（INMP441 模块，针序 SCK/SD/WS/L/R/VDD/GND）
  - J4：1x5 母排座（MAX98357 模块，VIN/GND/DIN/BCLK/LRC）
  - J5：1x4 母排座（SSD1306 OLED，GND/VCC/SCL/SDA）
  - J6：2P 直针（喇叭端子，飞线接 MAX98357 模块 SPK± 焊盘）
  - SW1 轻触按键（GPIO0）、D1 LED + R1 限流（GPIO38）
  - C1 100uF / C2 100nF（5V 电源滤波）
  - 背面整板 GND 覆铜（KiCad 6.0.2 的 ZONE_FILLER segfault 已在 10.0.6 修复）

引脚定义与固件 src/main/config.h 完全一致。
用法：python3 gen_pcb.py   （需 KiCad 10，产出 shield.kicad_pcb）
"""
import os
import sys
from collections import defaultdict

import pcbnew

BOARD_W = 62.0
BOARD_H = 68.0
FPC = os.environ.get("KICAD_FOOTPRINTS", "/usr/share/kicad/footprints")

MM = pcbnew.FromMM
def P(x, y):
    return pcbnew.VECTOR2I(MM(x), MM(y))

board = pcbnew.CreateEmptyBoard()


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
    for p in f.Pads():
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
    t.SetTextSize(pcbnew.VECTOR2I(MM(size), MM(size)))
    t.SetTextThickness(MM(thickness))
    t.SetTextAngleDegrees(rot)
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
J2 = fp("Connector_PinSocket_2.54mm", "PinSocket_1x06_P2.54mm_Vertical", "J2", 6.5, 2.0, 90.0)
J4 = fp("Connector_PinSocket_2.54mm", "PinSocket_1x05_P2.54mm_Vertical", "J4", 50.0, 8.0)
J5 = fp("Connector_PinSocket_2.54mm", "PinSocket_1x04_P2.54mm_Vertical", "J5", 50.0, 26.0)
J6 = fp("Connector_PinHeader_2.54mm", "PinHeader_1x02_P2.54mm_Vertical", "J6", 14.0, 62.0)
SW1 = fp("Button_Switch_THT", "SW_PUSH_6mm", "SW1", 26.0, 62.0)
D1 = fp("LED_THT", "LED_D5.0mm", "D1", 19.0, 62.0)
R1 = fp("Resistor_THT", "R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal", "R1", 43.0, 62.0)
C1 = fp("Capacitor_THT", "CP_Radial_D6.3mm_P2.50mm", "C1", 50.0, 45.0, 180.0)
C2 = fp("Capacitor_THT", "C_Disc_D5.0mm_W2.5mm_P2.50mm", "C2", 43.0, 42.0, 180.0)
C3 = fp("Capacitor_THT", "C_Disc_D5.0mm_W2.5mm_P2.50mm", "C3", 18.0, 5.5)
C4 = fp("Capacitor_THT", "C_Disc_D5.0mm_W2.5mm_P2.50mm", "C4", 39.5, 35.0, 90.0)
for (x, y) in [(4, 4), (58, 4), (4, 64), (58, 64)]:
    h = fp("MountingHole", "MountingHole_3.2mm_M3", "H", x, y)
    h.Reference().SetVisible(False)   # 隐藏位号丝印，避免出板边

# ==================== 网络定义（KiCad 10：NETINFO_ITEM + board.Add） ====================
nets = {}
def net(name):
    if name not in nets:
        n = pcbnew.NETINFO_ITEM(board, name)
        board.Add(n)
        nets[name] = n
    return nets[name]

def connect(f, pin, name):
    pad(f, pin).SetNet(net(name))

def connect_all(f, pin, name):
    for p in f.Pads():
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
# 按键（GPIO0 -> SW1 两个 1 脚，SW1 两个 2 脚 -> GND）
connect(J3, 15, "KEY"); connect_all(SW1, 1, "KEY")
connect_all(SW1, 2, "GND")
# 滤波电容
connect(C1, 1, "5V");  connect(C1, 2, "GND")
connect(C2, 1, "5V");  connect(C2, 2, "GND")
connect(C3, 1, "3V3"); connect(C3, 2, "GND")   # INMP441 去耦
connect(C4, 1, "3V3"); connect(C4, 2, "GND")   # OLED 去耦
# 喇叭端子 J6 不接网络（飞线接 MAX98357 模块 SPK± 焊盘）

# ==================== 走线 ====================
# 布局拓扑（v1 已验证交叉/间距；GND 由背面覆铜承担，不再显式走 GND）：
#   正面 F.Cu：麦克风 SCK/WS、功放三线（竖线 16/17/18，水平走 J3 焊盘间隙）、
#             OLED-SCL、LIGHT、LED_A、KEY（含左触片桥接）
#   背面 B.Cu：麦克风 SD、3V3 全段（顶部走廊 y=5）、5V、OLED-SDA（覆铜自动避让）
F, B = pcbnew.F_Cu, pcbnew.B_Cu
# ---- 正面：麦克风 ----
path(F, 0.3, [pt(J2, 1), (pt(J2, 1)[0], 15.62), pt(J1, 4)], "MIC_BCLK")               # SCK
path(F, 0.3, [pt(J2, 3), (pt(J2, 3)[0], 3.4), (13.2, 3.4), (13.2, 13.08), pt(J1, 3)], "MIC_WS") # WS
# ---- 正面：功放三线 ----
path(F, 0.3, [pt(J1, 6), (16, pt(J1, 6)[1]), (16, 11.81), (50, 11.81), (50, 13.08)], "AMP_DIN")
path(F, 0.3, [pt(J1, 7), (17, pt(J1, 7)[1]), (17, 14.35), (50, 14.35), (50, 15.62)], "AMP_BCLK")
path(F, 0.3, [pt(J1, 8), (18, pt(J1, 8)[1]), (18, 16.89), (50, 16.89), (50, 18.16)], "AMP_LRC")
# ---- 正面：OLED-SCL、LIGHT、LED_A、KEY ----
path(F, 0.3, [pt(J3, 7), (37, pt(J3, 7)[1]), (37, 31.08), pt(J5, 3)], "OLED_SCL")
path(F, 0.3, [pt(J3, 11), (36.5, pt(J3, 11)[1]), (36.5, 62), pt(R1, 1)], "LIGHT")
path(F, 0.3, [pt(R1, 2), (pt(R1, 2)[0], 64), (pt(D1, 1)[0], 64), pt(D1, 1)], "LED_A")
path(F, 0.3, [pt(J3, 15), (31, pt(J3, 15)[1]), (31, 62), (32.5, 62)], "KEY")
path(F, 0.3, [(26, 62), (26, 59), (32.5, 59), (32.5, 62)], "KEY")                     # 左触片桥接
# ---- 背面：麦克风 SD、3V3、5V、OLED-SDA ----
path(B, 0.3, [pt(J2, 2), (pt(J2, 2)[0], 0.65), (21.9, 0.65), (21.9, 19.43), (12, 19.43), pt(J1, 5)], "MIC_SD")
path(B, 0.3, [pt(J1, 1), (12, 6.5), (pt(J2, 5)[0], 6.5), pt(J2, 5)], "3V3")
path(B, 0.3, [pt(J2, 5), (pt(J2, 5)[0], 16.89), (10.6, 16.89), (10.6, 22), (36.5, 22), (36.5, 24), (48.6, 24), (48.6, 28.54), pt(J5, 2)], "3V3")
path(B, 0.3, [pt(C3, 1), (17.3, pt(C3, 1)[1]), (17.3, 2), pt(J2, 5)], "3V3")   # C3 接 INMP441 VDD
path(B, 0.3, [pt(C4, 1), (pt(C4, 1)[0], 37.2), (33.2, 37.2), (33.2, 21.97), (36.5, 21.97), (36.5, 22)], "3V3")   # C4 绕 J3 间隙接 3V3 走廊
path(B, 0.5, [pt(J3, 2), (37, pt(J3, 2)[1]), (37, 6.5), (54.5, 6.5), (54.5, 8), pt(J4, 1)], "5V")
path(B, 0.5, [(54.5, 8), (54.5, 45), (50, 45), (50, 42), pt(C2, 1)], "5V")
path(B, 0.3, [pt(C1, 2), (41, pt(C1, 2)[1])], "GND")   # 缝合：C1- 接入主铜皮
path(B, 0.3, [(10.9, 14.35), (14, 14.35)], "GND")    # 缝合：穿过 J1 间隙 y=14.35 打通孤岛
path(B, 0.3, [pt(J3, 8), (41, pt(J3, 8)[1]), (41, 33.62), pt(J5, 4)], "OLED_SDA")

# ==================== GND 缝合过孔 ====================
def gnd_via(x, y):
    v = pcbnew.PCB_VIA(board)
    v.SetPosition(P(x, y))
    v.SetDrill(MM(0.4))
    v.SetWidth(MM(0.8))
    v.SetNet(net("GND"))
    board.Add(v)
# 板边缝合（避开安装孔与器件）
for yy in [13, 20, 27, 34, 41, 48, 55]:
    gnd_via(3, yy); gnd_via(59, yy)
for xx in [24, 32, 40, 48]:
    gnd_via(xx, 3); gnd_via(xx, 65)
gnd_via(8, 65)
# 孤岛缝合链：B.Cu 孤岛 -> 过孔 -> F.Cu 短走线 -> J2.4（GND 焊盘回到主铜皮）
gnd_via(14.5, 12.5)
path(F, 0.3, [(14.5, 12.5), (14.12, 12.5), pt(J2, 4)], "GND")

# ==================== 背面整板 GND 覆铜 ====================
z = pcbnew.ZONE(board)
z.SetLayer(pcbnew.B_Cu)
board.Add(z)
z.SetNetCode(net("GND").GetNetCode())
z.Outline().NewOutline()
for (x, y) in [(1, 1), (BOARD_W - 1, 1), (BOARD_W - 1, BOARD_H - 1), (1, BOARD_H - 1)]:
    z.Outline().Append(P(x, y))
z.SetMinThickness(MM(0.25))
z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)   # 实心连接：免热焊盘辐条不足问题，排针手工焊接无碍
z.SetIsFilled(True)

# ==================== 边框、丝印 ====================
edge_rect(0, 0, BOARD_W, BOARD_H)
text("Voice_Buddy 盾板", 24, 3, 1.6)
text("INMP441", 7.0, 5.5)
text("MAX98357", 46.5, 20.5)
text("OLED", 46.5, 35.5)
text("SPK", 13.0, 64.8)
text("KEY", 24.0, 65.0)
text("LED", 19.0, 65.0)
text("GPIO38-LED / GPIO0-KEY", 24.0, 67.3, 0.9)
text("J1:1=3V3  J3:1=GND 2=5V", 12.0, 62.8, 0.9)
text("喇叭飞线: MAX98357模块 SPK+/- -> J6", 24.0, 34.0, 0.9)
text("USB 口朝下插接 DevKitC-1（底排元件已让位）", 24.0, 47.0, 0.9)

# ==================== 填充覆铜、保存、连通性自检 ====================
pcbnew.ZONE_FILLER(board).Fill(board.Zones())

out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "shield.kicad_pcb")
pcbnew.SaveBoard(out, board)

cnt = defaultdict(int)
for f in board.GetFootprints():
    for p in f.Pads():
        n = p.GetNetname()
        if n:
            cnt[n] += 1
warn = 0
for n, c in sorted(cnt.items()):
    if c < 2:
        print(f"WARN 网络只有 {c} 个焊盘: {n}")
        warn += 1

print(f"OK -> {out}")
print(f"元件 {len(board.GetFootprints())} 个，网络 {len(nets)} 个，走线 {len(board.GetTracks())} 段，覆铜 {len(board.Zones())} 块，孤立网络 {warn} 个")
