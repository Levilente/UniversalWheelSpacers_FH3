# Universal Wheel Spacers (FH3)

A mod for the [FH3 Mod Platform](https://github.com/Levilente/FH3_ModPlatform). It adds two upgrades to every car under
**Garage > Upgrades > Tires and Rims**:

- **Front Spacers**
- **Rear Spacers**

Each has the stock stage (fitted on every car by default) and the stages **+1 cm to +9 cm per wheel**, 300 CR each. The stage is
stored per owned car, so two cars of the same model can have different spacers.

> Works in the game. Needs FH3 Mod Platform 1.0.0 (API 12) from 6 Nov 2026 or newer: the shop preview, the basket and the wheel camera come from it.

## Requirements

- Forza Horizon 3 through Azimuth with the FH3 Mod Platform 1.0.0 (API 12) or newer.

## Install

Copy `WheelSpacers.dll` into the `fh3-mods` folder next to `azimuth.exe`. On the first start the mod writes
`fh3-mods\WheelSpacers.ini`.

## Settings (`fh3-mods\WheelSpacers.ini`)

| Key | Default | Meaning |
|---|---|---|
| `Scale` | `1.0` | Multiplies the spacer width (1 cm per stage per wheel). |
| `DebugLog` | `0` | `1` logs the cars the game builds and what the mod changed to `fh3-mods\platform.log` (for bug reports). |

## In the shop

- The cursor on a stage shows it on the car before it is bought; a stage in the basket stays on the car until the setup is installed or abandoned.
- The camera goes to the front wheel in Front Spacers and to the rear wheel in Rear Spacers, as it does for rim size.

## How it works

The game's central car loader (`game+0x19E6A80`) reads `ModelFrontTrackOuter` / `ModelRearTrackOuter` (metres, outer tire edge to
outer tire edge) into the car spec at `+0xF4` / `+0xF8`. The mod hooks the instruction right after (`game+0x19E9027`), asks the platform
for the car's front and rear stage (`upgrade_stage`) and adds twice the spacer width. From there the game computes the track the physics
and the model use, so the wheels move out and the car really is wider. The loader runs every time the game builds a car, including the
shop's car on each cursor move, which is why the stage shows on hover (platform 1.0.2). Cars other than the one in the garage slot are
left alone (the model id is checked).

## Icons

The upgrade tiles use the spacer icons of the FH2 [Universal Wheel Spacers](https://github.com/Levilente/Universal-Wheel-Spacers):
`icons/front.png` and `icons/rear.png` for the two upgrade types, `icons/spacer.png` for the +1 to +9 cm stages (Stock keeps the
game's stock icon; the large picture is the game's Tires and Rims one). They are 200 x 200 RGBA PNGs. To change one, replace the PNG
and run

```sh
pip install pillow numpy
python3 scripts/gen_icons.py --prefix uws
```

which writes `src/icon_data.h` (the icons as FH3 `.swatchbin`, packed into the DLL); then rebuild. At start the mod hands them to the
game with the platform's `set_file` as `Upgrade_Parts\Custom\uws_<slot>_Icon.swatchbin`; no game file is changed. The converter
(`scripts/icontools/`) comes from NitroSystem II for FH3.

## Build

```sh
git submodule add https://github.com/Levilente/FH3_ModPlatform platform   # once
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=platform/cmake/mingw-w64-x86_64.cmake
cmake --build build
```

The result is `build/WheelSpacers.dll` (llvm-mingw, see the platform's `docs/writing-a-mod.md`).

## Limits

- The upgrade bridge allows 10 stages per upgrade, so stock plus +1 to +9 cm.
