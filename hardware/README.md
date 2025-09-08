# Hardware projects

This folder hosts independent KiCad projects for each board:

- led-panel/ — LED segment panel PCB
- controller/ — main controller PCB
- rpi-hat/ — Raspberry Pi HAT
- power/ — power board
- lib/ — shared symbols/footprints

Recommendations
- Keep each board as an independent KiCad project (.kicad_pro, .kicad_sch, .kicad_pcb)
- Use relative library paths (e.g., ${KIPRJMOD}/../lib) to avoid absolute path issues
- Put generated outputs (Gerber/Drill/PnP) in each board's outputs/ and attach to Releases
- Consider CERN OHL for hardware license
