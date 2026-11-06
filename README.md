# Universal Wheel Spacers (FH3)

A mod for the [FH3 Mod Platform](https://github.com/Levilente/FH3_ModPlatform). It adds two upgrades to every car under
**Garage > Upgrades > Tires and Rims**:

- **Front Spacers**
- **Rear Spacers**

Each has the stock stage (fitted on every car by default) and the stages **+1 cm to +9 cm per wheel**, 300 CR each. The stage is
stored per owned car, so two cars of the same model can have different spacers.

> Status: works in the game (0.4.3). Needs FH3 Mod Platform 1.0.1 or newer (1.0.0 let the shop overwrite the car's tuning, which took the handbrake away); with 1.0.2 the spacer shows on the car while you look at a stage in the shop.

## Requirements

- Forza Horizon 3 through Azimuth with the FH3 Mod Platform 1.0.1 or newer (1.0.2 for the shop preview).

## Install

Copy `WheelSpacers.dll` into the `fh3-mods` folder next to `azimuth.exe`. On the first start the mod writes
`fh3-mods\WheelSpacers.ini`.

## Settings (`fh3-mods\WheelSpacers.ini`)

| Key | Default | Meaning |
|---|---|---|
| `Scale` | `1.0` | Multiplies the spacer width. Use `-1` if the wheels move inwards. |
| `ApplyBody` | `0` | Off by default: with it on the handbrake stopped working. Add twice the spacer width to the car body's `ModelFrontTrackOuter` / `ModelRearTrackOuter` (this is what moves the wheels). |
| `ApplyOffsets` | `0` | Also add the width to the four `CarTrackOffsets` values (the game doesn't seem to use them for the wheels). |
| `Log` | `1` | Log every car the game builds to `fh3-mods\platform.log`. |
| `Trace` | `1` | Diagnostics for the shop preview work: logs SQL statements about upgrades, the shop's A / add steps and the game's call stack for every car it builds (capped). |
| `Register` | `1` | Test switch: `0` loads the DLL but registers no upgrades and installs no hooks. |
| `TraceShopHooks` | `0` | Also hook the shop functions the platform hooks (SQL prepare, A handler, add step). Off by default: with it on, the spacer upgrades vanished from the menu. |

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
