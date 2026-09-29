#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_pcb.py — Voice_Buddy 盾板（载板）PCB 生成脚本（KiCad 10 版，v3）
==================================================
用 pcbnew Python API 从零生成盾板布局：
  - J1/J3：1x19 母排座（插接 ESP32-S3 DevKitC-1，中心距 22.86mm）
  - J2：2x3 双排母座（圆形 INMP441 模块，左列上→下 L/R,WS,SCK；右列上→下 GND,VDD,SD）
  - J4：1x7 单排母座（MAX98357A 模块，上→下 UIN,GND,SD,GAIN,DIN,BCLK,LRC）
  - J5：1x4 单排母座（SSD1306 OLED，上→下 GND,VCC,SCL,SDA）
  - J6：2P 直针（喇叭端子，飞线接 MAX98357 模块绿色端子）
  - SW1 轻触按键（GPIO0）、D1 LED + R1 限流（GPIO38）
  - C1 100uF（3V3 大容量滤波）、C2/C3/C4 100nF（三模块就近去耦，跨 3V3↔GND）
  - R2/R3 4.7k 可选上拉（OLED SCL/SDA→3V3，默认不焊，模块板载上拉则留空）
  - 背面整板 GND 覆铜（实心连接）+ 板边缝合过孔
  - 电源：全部 3V3（无 5V 网络）；I2S0/I2S1 时钟独立

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
J2 = fp("Connector_PinSocket_2.54mm", "PinSocket_2x03_P2.54mm_Vertical", "J2", 7.4, 9.5, 180.0)
J4 = fp("Connector_PinSocket_2.54mm", "PinSocket_1x07_P2.54mm_Vertical", "J4", 50.0, 8.0)
J5 = fp("Connector_PinSocket_2.54mm", "PinSocket_1x04_P2.54mm_Vertical", "J5", 50.0, 26.0)
J6 = fp("Connector_PinHeader_2.54mm", "PinHeader_1x02_P2.54mm_Vertical", "J6", 14.0, 62.0)
SW1 = fp("Button_Switch_THT", "SW_PUSH_6mm", "SW1", 26.0, 62.0)
D1 = fp("LED_THT", "LED_D5.0mm", "D1", 19.0, 62.0)
R1 = fp("Resistor_THT", "R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal", "R1", 43.0, 62.0)
C1 = fp("Capacitor_THT", "CP_Radial_D6.3mm_P2.50mm", "C1", 50.0, 45.0, 180.0)
C2 = fp("Capacitor_THT", "C_Disc_D5.0mm_W2.5mm_P2.50mm", "C2", 52.0, 4.5, 90.0)
C3 = fp("Capacitor_THT", "C_Disc_D5.0mm_W2.5mm_P2.50mm", "C3", 23.5, 3.5)
C4 = fp("Capacitor_THT", "C_Disc_D5.0mm_W2.5mm_P2.50mm", "C4", 42.0, 36.0)
for (x, y) in [(4, 4), (58, 4), (4, 64), (58, 64)]:
    h = fp("MountingHole", "MountingHole_3.2mm_M3", "H", x, y)
    h.Reference().SetVisible(False)

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

# 电源：全部 3V3（无 5V 网络，规范：UIN=3V3）
connect(J1, 1, "3V3")
# J2 圆形 INMP441（左列上→下 L/R,WS,SCK = pin1/3/5；右列上→下 GND,VDD,SD = pin2/4/6）
connect(J2, 1, "GND")                      # pin1 L/R 接地选左声道
connect(J2, 2, "GND")                      # pin2 GND
connect(J1, 3, "MIC_WS");   connect(J2, 3, "MIC_WS")     # pin3 WS → GPIO4
connect(J2, 4, "3V3")                                     # pin4 VDD
connect(J1, 4, "MIC_BCLK"); connect(J2, 5, "MIC_BCLK")   # pin5 SCK → GPIO5
connect(J1, 5, "MIC_SD");   connect(J2, 6, "MIC_SD")     # pin6 SD → GPIO6
# J4 MAX98357A（上→下 UIN,GND,SD,GAIN,DIN,BCLK,LRC = pin1..7）
connect(J4, 1, "3V3")                                     # pin1 UIN（3.3V 供电）
connect(J4, 2, "GND")                                    # pin2 GND
# pin3 SD 悬空（静音控制，悬空=不静音）；pin4 GAIN 悬空（默认增益）
connect(J1, 6, "AMP_DIN");  connect(J4, 5, "AMP_DIN")     # pin5 DIN → GPIO7
connect(J1, 7, "AMP_BCLK"); connect(J4, 6, "AMP_BCLK")    # pin6 BCLK → GPIO15
connect(J1, 8, "AMP_LRC");  connect(J4, 7, "AMP_LRC")     # pin7 LRC → GPIO16
# J5 SSD1306 OLED（上→下 GND,VCC,SCL,SDA）
connect(J5, 1, "GND")
connect(J5, 2, "3V3")
connect(J3, 7, "OLED_SCL"); connect(J5, 3, "OLED_SCL")
connect(J3, 8, "OLED_SDA"); connect(J5, 4, "OLED_SDA")
# LED + 电阻（GPIO38 -> R1 -> D1 -> GND）
connect(J3, 11, "LIGHT"); connect(R1, 1, "LIGHT")
connect(R1, 2, "LED_A");   connect(D1, 1, "LED_A")
connect(D1, 2, "GND")
# 按键（GPIO0 -> SW1 两个 1 脚，SW1 两个 2 脚 -> GND）
connect(J3, 15, "KEY")
for p in SW1.Pads():
    if p.GetNumber() == "1":
        p.SetNet(net("KEY"))
    elif p.GetNumber() == "2":
        p.SetNet(net("GND"))
# 滤波/去耦：C1 100uF 3V3 大容量；C2(J4)/C3(J2)/C4(J5) 100nF 跨 3V3↔GND
connect(C1, 1, "3V3");  connect(C1, 2, "GND")
connect(C2, 1, "3V3");  connect(C2, 2, "GND")
connect(C3, 1, "3V3");  connect(C3, 2, "GND")
connect(C4, 1, "3V3");  connect(C4, 2, "GND")
# 喇叭端子 J6 不接网络（飞线接 MAX98357 模块绿色端子）

# ==================== 走线 ====================
F, B = pcbnew.F_Cu, pcbnew.B_Cu
# ---- 正面：麦克风 SCK/WS ----
path(F, 0.3, [pt(J2, 5), (pt(J2, 5)[0], 0.65), (14.4, 0.65), (14.4, 15.62), pt(J1, 4)], "MIC_BCLK")  # SCK 底边走廊
path(F, 0.3, [pt(J2, 3), (pt(J2, 3)[0], 5.7), (13.25, 5.7), (13.25, 13.08), pt(J1, 3)], "MIC_WS")   # WS
# ---- 正面：功放三线（水平走 J3 焊盘间隙，末端折入 J4 pin5/6/7）----
path(F, 0.3, [pt(J1, 6), (16, pt(J1, 6)[1]), (16, 16.89), (50, 16.89), (50, 18.16)], "AMP_DIN")      # DIN -> pin5
path(F, 0.3, [pt(J1, 7), (17, pt(J1, 7)[1]), (17, 21.97), (50, 21.97), (50, 20.7)], "AMP_BCLK")     # BCLK -> pin6
path(F, 0.3, [pt(J1, 8), (18, pt(J1, 8)[1])], "AMP_LRC")   # LRC 正面段
vlrc = pcbnew.PCB_VIA(board)
vlrc.SetPosition(P(18, 25.78))
vlrc.SetDrill(MM(0.4))
vlrc.SetWidth(MM(0.8))
vlrc.SetNet(net("AMP_LRC"))
board.Add(vlrc)
path(B, 0.3, [(18, 25.78), (18, 39.75), (52.3, 39.75), (52.3, 23.24), pt(J4, 7)], "AMP_LRC")   # LRC 背面段
# ---- 正面：OLED-SCL、LIGHT、LED_A、KEY ----
path(F, 0.3, [pt(J3, 7), (37, pt(J3, 7)[1]), (37, 31.08), pt(J5, 3)], "OLED_SCL")
path(F, 0.3, [pt(J3, 11), (36.5, pt(J3, 11)[1]), (36.5, 62), pt(R1, 1)], "LIGHT")
path(F, 0.3, [pt(R1, 2), (pt(R1, 2)[0], 64), (pt(D1, 1)[0], 64), pt(D1, 1)], "LED_A")
path(F, 0.3, [pt(J3, 15), (31, pt(J3, 15)[1]), (31, 62), (32.5, 62)], "KEY")
path(F, 0.3, [(26, 62), (26, 59), (32.5, 59), (32.5, 62)], "KEY")   # 左触片桥接
# ---- 背面：3V3 顶部走廊（J1.1 -> J4.1 UIN + VDD 支路 + OLED 支路 + C2/C3）----
path(B, 0.3, [pt(J1, 1), (12, 7.7), (8.68, 7.7), (8.68, 21.97), (52.3, 21.97)], "3V3")     # 主干
path(B, 0.3, [(52.3, 21.97), (52.3, 8), pt(J4, 1)], "3V3")                                   # UIN(pin1)
path(B, 0.3, [(8.68, 7.7), (10.3, 7.7), (10.3, 7.8), (pt(J2, 4)[0], 7.8), pt(J2, 4)], "3V3")  # VDD(pin4)
path(B, 0.3, [pt(C2, 1), (pt(C2, 1)[0], 21.97)], "3V3")                                        # C2 去耦
path(B, 0.3, [pt(C3, 1), (pt(C3, 1)[0], 21.97)], "3V3")                            # C3 去耦
# ---- 背面：麦克风 SD（y=1.2 走廊 + x=21.9 竖线 + y=19.43 进 J1.5）----
path(B, 0.3, [pt(J2, 6), (pt(J2, 6)[0], 1.2), (21.9, 1.2), (21.9, 19.43), (12, 19.43), pt(J1, 5)], "MIC_SD")
# ---- 背面：OLED 3V3 支路（无过孔，直接分叉）----
path(B, 0.3, [(22.6, 21.97), (24, 21.97), (24, 22), (36.5, 22), (36.5, 24), (48.0, 24), (48.0, 28.54), pt(J5, 2)], "3V3")
# ---- 背面：C1 大容量滤波（3V3）----
path(F, 0.3, [pt(C1, 1), (54, pt(C1, 1)[1]), (54, 28.54), pt(J5, 2)], "3V3")
# ---- 背面：C4 去耦（J5 旁）----
path(B, 0.3, [pt(C4, 1), (pt(C4, 1)[0], 37.2), (33.0, 37.2), (33.0, 22), (36.5, 22)], "3V3")
path(B, 0.3, [pt(C4, 2), (pt(C4, 2)[0], 38.5), (41, 38.5)], "GND")   # C4- 缝合出孤岛
# ---- 背面：OLED-SDA ----
path(B, 0.3, [pt(J3, 8), (41, pt(J3, 8)[1]), (41, 33.62), pt(J5, 4)], "OLED_SDA")
# ---- 背面：R2/R3 可选上拉走线（DNP 位）----

# ==================== GND 缝合过孔 ====================
def gnd_via(x, y):
    v = pcbnew.PCB_VIA(board)
    v.SetPosition(P(x, y))
    v.SetDrill(MM(0.4))
    v.SetWidth(MM(0.8))
    v.SetNet(net("GND"))
    board.Add(v)
for yy in [13, 20, 27, 34, 41, 48, 55]:
    gnd_via(3, yy); gnd_via(59, yy)
for xx in [24, 32, 40, 48]:
    gnd_via(xx, 3); gnd_via(xx, 65)
gnd_via(8, 65)

# ==================== 背面整板 GND 覆铜 ====================
z = pcbnew.ZONE(board)
z.SetLayer(pcbnew.B_Cu)
board.Add(z)
z.SetNetCode(net("GND").GetNetCode())
z.Outline().NewOutline()
for (x, y) in [(1, 1), (BOARD_W - 1, 1), (BOARD_W - 1, BOARD_H - 1), (1, BOARD_H - 1)]:
    z.Outline().Append(P(x, y))
z.SetMinThickness(MM(0.25))
z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)   # 实心连接
z.SetIsFilled(True)

# ==================== 边框、丝印 ====================
edge_rect(0, 0, BOARD_W, BOARD_H)
text("Voice_Buddy 盾板", 24, 3, 1.6)
text("INMP441(2x3) 左列:L/R,WS,SCK 右列:GND,VDD,SD", 6.0, 13.0, 0.9)
text("MAX98357A(1x7) UIN,GND,SD,GAIN,DIN,BCLK,LRC", 45.5, 25.0, 0.9)
text("OLED", 46.5, 35.5)
text("SPK", 13.0, 64.8)
text("LED", 19.0, 65.0)
text("KEY", 24.0, 65.0)
text("GPIO38-LED / GPIO0-KEY", 24.0, 67.3, 0.9)
text("OLED 需板载 I2C 上拉（常见模块均有）", 30.0, 30.0, 0.9)
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
