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

## 重新生成

```bash
python3 gen_pcb.py      # 生成/修改 shield.kicad_pcb
python3 export.py       # 导出 preview/ 与 gerbers/
```

> 注意：KiCad 6.0.2 的 ZONE_FILLER 有 segfault bug（本板因此未用覆铜，
> GND 网络全部显式走线）。若换 KiCad 7/8，gen_pcb.py 中个别 API 名需调整
> （如 Pads()→GetPads()、wxPoint→VECTOR2I、F_SilkS→F_Silkscreen）。

## 打样参数（嘉立创 JLCPCB）

- 板尺寸：62 × 64 mm，2 层，板厚 1.6mm
- 上传 gerbers/ 目录（zip 后上传或逐文件上传均可）
- 钻孔文件：shield-PTH.drl 与 shield-NPTH.drl 都要传
- 最小线宽 0.3mm、最小间距 0.2mm、最小过孔 0.4mm 孔/0.8mm 盘——常规工艺即可，无需加钱

## 元件清单

见 [设计说明.md](设计说明.md) 第 3 节（全部为直插元件，1×19 母排座 ×2 等）。
