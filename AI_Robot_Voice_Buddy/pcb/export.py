#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
export.py — 导出盾板预览图（SVG）与 Gerber 打样文件（KiCad 6 兼容）
用法：python3 export.py
产出：preview/（SVG 预览）、gerbers/（Gerber + 钻孔）
"""
import os
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
board = pcbnew.LoadBoard(os.path.join(HERE, "shield.kicad_pcb"))

# ---------- SVG 预览（各层单独渲染，便于检查） ----------
prev = os.path.join(HERE, "preview")
os.makedirs(prev, exist_ok=True)
layers = [
    (pcbnew.F_Cu, "F_Cu"),
    (pcbnew.B_Cu, "B_Cu"),
    (pcbnew.F_SilkS, "F_SilkS"),
    (pcbnew.Edge_Cuts, "Edge_Cuts"),
]
for layer, name in layers:
    po = pcbnew.PLOT_CONTROLLER(board)
    opts = po.GetPlotOptions()
    opts.SetOutputDirectory(prev)
    opts.SetPlotFrameRef(False)
    opts.SetMirror(False)
    opts.SetNegative(False)
    po.SetLayer(layer)
    po.OpenPlotfile(name, pcbnew.PLOT_FORMAT_SVG, name)
    po.PlotLayer()
    po.ClosePlot()
print("SVG 预览已输出到", prev)

# ---------- Gerber ----------
ger = os.path.join(HERE, "gerbers")
os.makedirs(ger, exist_ok=True)
gerber_layers = [
    (pcbnew.F_Cu, "F_Cu.gtl"),
    (pcbnew.B_Cu, "B_Cu.gbl"),
    (pcbnew.F_SilkS, "F_SilkS.gto"),
    (pcbnew.B_SilkS, "B_SilkS.gbo"),
    (pcbnew.F_Mask, "F_Mask.gts"),
    (pcbnew.B_Mask, "B_Mask.gbs"),
    (pcbnew.Edge_Cuts, "Edge_Cuts.gm1"),
]
po = pcbnew.PLOT_CONTROLLER(board)
opts = po.GetPlotOptions()
opts.SetOutputDirectory(ger)
opts.SetPlotFrameRef(False)
opts.SetSketchPadLineWidth(pcbnew.FromMM(0.1) if hasattr(pcbnew, "FromMM") else 100000)
opts.SetAutoScale(False)
opts.SetScale(1)
opts.SetMirror(False)
opts.SetExcludeEdgeLayer(True)
opts.SetPlotReference(True)
opts.SetPlotValue(True)
opts.SetUseGerberAttributes(False)
for layer, fname in gerber_layers:
    po.SetLayer(layer)
    po.OpenPlotfile(fname.replace(".", "-"), pcbnew.PLOT_FORMAT_GERBER, fname)
    po.PlotLayer()
    po.ClosePlot()
    # KiCad 6 实际文件名带板名前缀和后缀 .gbr，重命名为标准名
    src = os.path.join(ger, "shield-" + fname.replace(".", "-") + ".gbr")
    if os.path.exists(src):
        os.rename(src, os.path.join(ger, fname))
print("Gerber 已输出到", ger)

# ---------- 钻孔 ----------
drl = os.path.join(HERE, "gerbers")
ew = pcbnew.EXCELLON_WRITER(board)
ew.SetFormat(True, pcbnew.EXCELLON_WRITER.DECIMAL_FORMAT, 3, 3)
ew.SetOptions(False, True, pcbnew.wxPoint(0, 0), False)
ew.CreateDrillandMapFilesSet(drl, True, False)
print("钻孔文件已输出到", drl)
print("OK")
