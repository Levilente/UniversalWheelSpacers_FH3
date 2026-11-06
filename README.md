# Universal Wheel Spacers (FH3)

A mod for the [FH3 Mod Platform](https://github.com/Levilente/FH3_ModPlatform). It adds two upgrades to every car under
**Garage > Upgrades > Tires and Rims**:

- **Front Spacers**
- **Rear Spacers**

Each has the stock stage (fitted on every car by default) and the stages **+1 cm to +9 cm per wheel**, 300 CR each. The stage is
stored per owned car, so two cars of the same model can have different spacers.

> Status: version 0.1.1 is the first build that applies the width. It was written from reading the game's code and has not been run in the game yet.

## Requirements

- Forza Horizon 3 through Azimuth with the FH3 Mod Platform (API 11 or newer).

## Install

Copy `WheelSpacers.dll` into the `fh3-mods` folder next to `azimuth.exe`. On the first start the mod writes
`fh3-mods\WheelSpacers.ini`.

## Settings (`fh3-mods\WheelSpacers.ini`)

| Key | Default | Meaning |
|---|---|---|
| `Scale` | `1.0` | Multiplies the spacer width. Use `-1` if the wheels move inwards. |
| `ApplyStock` | `1` | Add the width to the stock track offsets (`FrontOffset`, `RearOffset`). |
| `ApplyUpgrade` | `1` | Add it to the aftermarket-wheel offsets (`UpgradeFrontOffset`, `UpgradeRearOffset`). |
| `Log` | `1` | Log every car the game builds to `fh3-mods\platform.log`. |

## How it works

When the game builds a car's data object (`game+0x189F020`) it reads the car's row of the database table `CarTrackOffsets`
and stores four floats (metres) at `+0x773C`, `+0x7740`, `+0x7744` and `+0x7748` of that object. The mod hooks the instruction
after those stores (`game+0x189F8B8`) and adds the spacer width of the car being driven.

## Build

```sh
git submodule add https://github.com/Levilente/FH3_ModPlatform platform   # once
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=platform/cmake/mingw-w64-x86_64.cmake
cmake --build build
```

The result is `build/WheelSpacers.dll` (llvm-mingw, see the platform's `docs/writing-a-mod.md`).

## Limits

- The upgrade bridge allows 10 stages per upgrade, so stock plus +1 to +9 cm.
