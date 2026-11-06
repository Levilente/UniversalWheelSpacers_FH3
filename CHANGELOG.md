# Changelog

## 1.0.1

- A car chosen at an event's car select gets its spacers (it was taken for another car until the game saved it as the driven car, and kept no spacers until a spacer was changed in the garage; needs platform API 14). Any of your own cars keeps its spacers; only cars that aren't yours (loaners, AI cars) get none.
- Spacers now work on cars whose game id is below 1000 (84 cars, among them the Nissan Skyline GT-R V-Spec II (R34) and the older Skylines and Silvias); the mod took such ids for unreadable values and left the car alone.

## 1.0.0

First release of **Universal Wheel Spacers for Forza Horizon 3**, on the [FH3 Mod Platform](https://github.com/Levilente/FH3_ModPlatform) 1.0.0 (API 12). A port of the FH2 [Universal Wheel Spacers](https://github.com/Levilente/Universal-Wheel-Spacers).

- Two upgrades on every car, **Garage > Upgrades > Tires and Rims > Front Spacers** and **Rear Spacers**: Stock and **+1 cm to +9 cm per wheel**, 300 CR a stage, registered through the platform's upgrade bridge and stored per owned car.
- The spacer width is added to the car's track where the game's car loader reads it (`ModelFrontTrackOuter` / `ModelRearTrackOuter`), so the wheels move out and the car really is wider, physics and model alike.
- The shop shows a stage on the car when the cursor is on it, and keeps a stage in the basket on the car until the setup is installed or abandoned. The camera goes to the front or rear wheel.
- Stages you own go into the basket at 0 CR; the shop's own credit check counts the basket (the game's "not enough credits" message).
- Cars other than the one in the garage slot are left alone (a mission loaner gets no spacers).
- Spacer icons from the FH2 mod, handed to the game with the platform's `set_file` (no game file is changed).
- `fh3-mods\WheelSpacers.ini`: `Scale` (spacer width multiplier) and `DebugLog`.
