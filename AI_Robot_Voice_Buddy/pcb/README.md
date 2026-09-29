# Voice_Buddy 盾板 PCB

本目录是 Voice_Buddy 盾板（载板）的完整设计产物。设计说明与装配方法见 [设计说明.md](设计说明.md)。

## 文件清单

| 文件 | 说明 |
|---|---|
| shield.kicad_pcb | KiCad 6 板卡文件（可打开修改） |
| gen_pcb.py | 板卡生成脚本（Python + KiCad 6 pcbnew API） |
| export.py | 导出脚本：SVG 预览 + Gerber + 钻孔 |
| gerbers/ | 打样文件（7 层 Gerber + 2 个钻孔文件） |
| preview/ | 各层 SVG 预览（F_Cu/B_Cu/丝印/边框） |

## 重新生成（KiCad 10.0.6）

```bash
python3 gen_pcb.py                          # 生成/修改 shield.kicad_pcb
kicad-cli pcb drc --refill-zones shield.kicad_pcb   # DRC 校验
kicad-cli pcb render --side top/bottom -o preview/x.png shield.kicad_pcb  # 渲染预览
cd gerbers && kicad-cli pcb export gerbers -o . --layers F.Cu,B.Cu,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts ../shield.kicad_pcb && kicad-cli pcb export drill -o . ../shield.kicad_pcb
```

> v2 为 KiCad 10 重制版：背面整板 GND 覆铜 + 官方 DRC 校验。export.py 为
> KiCad 6 时代的旧导出脚本，已由 kicad-cli 取代，可忽略。

## 打样参数（嘉立创 JLCPCB）

- 板尺寸：62 × 64 mm，2 层，板厚 1.6mm
- 上传 gerbers/ 目录（zip 后上传或逐文件上传均可）
- 钻孔文件：shield.drl 一并上传（含 PTH/NPTH）
- 最小线宽 0.3mm、最小间距 0.2mm、最小过孔 0.4mm 孔/0.8mm 盘——常规工艺即可，无需加钱

## 元件清单

见 [设计说明.md](设计说明.md) 第 3 节（全部为直插元件，1×19 母排座 ×2 等）。
