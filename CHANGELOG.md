# Changelog

## 1.0.0

First release of **Universal Wheel Spacers for Forza Horizon 3**, on the [FH3 Mod Platform](https://github.com/Levilente/FH3_ModPlatform) 1.0.0. A port of the FH2 [Universal Wheel Spacers](https://github.com/Levilente/Universal-Wheel-Spacers).

- Two upgrades on every car, **Garage > Upgrades > Tires and Rims > Front Spacers** and **Rear Spacers**: Stock and **+1 cm to +9 cm per wheel**, 300 CR a stage, registered through the platform's upgrade bridge and stored per owned car.
- The spacer width is added to the car's track where the game's car loader reads it (`ModelFrontTrackOuter` / `ModelRearTrackOuter`), so the wheels move out and the car really is wider, physics and model alike.
- The shop shows a stage on the car when the cursor is on it, and keeps a stage in the basket on the car until the setup is installed or abandoned. The camera goes to the front or rear wheel.
- Stages you own go into the basket at 0 CR; the shop's own credit check counts the basket (the game's "not enough credits" message).
- Spacers only on your own cars, decided by the platform for every car the game builds: a car chosen at an event's car select has its spacers from the start; AI cars of the same model (one-make races), showcase cars and loaners get none.
- Works on every car, including the 84 whose game id is below 1000 (the Nissan Skyline GT-R V-Spec II (R34), the older Skylines and Silvias).
- Spacer icons from the FH2 mod, handed to the game with the platform's `set_file` (no game file is changed).
- `fh3-mods\WheelSpacers.ini`: `Scale` (spacer width multiplier) and `DebugLog` (off by default: only problems are logged).
