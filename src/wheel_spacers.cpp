// Universal Wheel Spacers for Forza Horizon 3 (FH3 Mod Platform mod).
//
// Adds two upgrades under Garage > Upgrades > Tires and Rims: "Front Spacers" and "Rear Spacers".
// Stage 0 is stock (fitted on every car by default), the other stages are +1 cm, +2 cm, ... per wheel
// for 300 CR each. The stage is stored per owned car (garage id) by the platform's upgrade bridge.
//
// How the track width is changed: when the game builds a car's data object (function game+0x189F020) it reads
// the car's row of the database table CarTrackOffsets and stores FrontOffset, RearOffset, UpgradeFrontOffset and
// UpgradeRearOffset (metres) at +0x773C, +0x7740, +0x7744 and +0x7748 of that object. This mod hooks the first
// instruction after those four stores (game+0x189F8B8) and adds the spacer width to them.
//
// Status: first version, written from reading the game's code. The log lines show every step.
#include "fh3mod.h"

#include <windows.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cwchar>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr int kStages            = 10;      // stage 0 (stock) + 1..9 cm: the upgrade bridge allows 10 stages at most
constexpr int kPrice             = 300;     // credits per spacer stage
constexpr uint32_t kHookRva      = 0x189F8B8;   // `mov rcx, [rdi+0x71E8]`, right after the four track offset stores
constexpr size_t kOffFront       = 0x773C;  // FrontOffset, RearOffset, UpgradeFrontOffset, UpgradeRearOffset (4 floats)
constexpr size_t kOffModelInfo   = 0x110;   // car -> record whose +0x30 is the car's model id (Data_Car.Id)
constexpr size_t kOffModelId     = 0x30;
const unsigned char kHookBytes[] = {0x48, 0x8B, 0x8F, 0xE8, 0x71, 0x00, 0x00};

const FH3ModApi* g_api = nullptr;
FH3Mod* g_mod          = nullptr;
int g_front            = -1;
int g_rear             = -1;

// fh3-mods\WheelSpacers.ini (written with these defaults on first start).
float g_scale     = 1.0f;   // multiplies the offset; -1 turns the wheels the other way if they move inwards
bool g_apply_stock = true;  // add to FrontOffset / RearOffset
bool g_apply_upgr  = true;  // add to UpgradeFrontOffset / UpgradeRearOffset
bool g_verbose     = true;  // log every car the game builds (the first 60) and every change

std::atomic<int> g_calls{0};
std::wstring g_cfg_path, g_platform_ini;

std::vector<std::wstring> g_names[2], g_descs[2];
FH3UpgradeStage g_stage_defs[2][kStages];

void say(const char* fmt, ...) {
    char line[300];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    if (g_api && g_mod) g_api->log(g_mod, line);
}

std::wstring wnum(long long v) { return std::to_wstring(v); }

// The platform keeps "stage|owned mask|model id" per garage id in its own ini; the model id is what lets this mod
// tell the player's car from other cars of the same build.
int stored_model(const wchar_t* section, uint32_t garage) {
    wchar_t buf[96] = L"";
    GetPrivateProfileStringW(section, wnum(garage).c_str(), L"", buf, 96, g_platform_ini.c_str());
    long a = 0, b = 0, c = 0;
    if (buf[0] && std::swscanf(buf, L"%ld|%ld|%ld", &a, &b, &c) >= 3) return static_cast<int>(c);
    return 0;
}

void load_config() {
    wchar_t buf[64] = L"";
    GetPrivateProfileStringW(L"WheelSpacers", L"Scale", L"", buf, 64, g_cfg_path.c_str());
    if (!buf[0]) {   // first start: write the defaults so the file is there to edit
        WritePrivateProfileStringW(L"WheelSpacers", L"Scale", L"1.0", g_cfg_path.c_str());
        WritePrivateProfileStringW(L"WheelSpacers", L"ApplyStock", L"1", g_cfg_path.c_str());
        WritePrivateProfileStringW(L"WheelSpacers", L"ApplyUpgrade", L"1", g_cfg_path.c_str());
        WritePrivateProfileStringW(L"WheelSpacers", L"Log", L"1", g_cfg_path.c_str());
        return;
    }
    g_scale        = static_cast<float>(std::wcstod(buf, nullptr));
    g_apply_stock = GetPrivateProfileIntW(L"WheelSpacers", L"ApplyStock", 1, g_cfg_path.c_str()) != 0;
    g_apply_upgr  = GetPrivateProfileIntW(L"WheelSpacers", L"ApplyUpgrade", 1, g_cfg_path.c_str()) != 0;
    g_verbose     = GetPrivateProfileIntW(L"WheelSpacers", L"Log", 1, g_cfg_path.c_str()) != 0;
}

void on_stage(void* user, uint32_t garage, int stage) {
    if (g_verbose) say("%s spacers: garage %u now has stage %d (+%d cm)", user ? "rear" : "front", garage, stage, stage);
}

// Runs on the game's thread every time the game builds a car's data object, before `mov rcx, [rdi+0x71E8]`.
// rdi is that object, and the four track offsets (floats, metres) are already stored in it.
void on_car_built(FH3Context* ctx, void*) {
    char* car = reinterpret_cast<char*>(ctx->rdi);
    int model = 0;
    __try {
        const char* info = *reinterpret_cast<char* const*>(car + kOffModelInfo);
        model            = *reinterpret_cast<const int*>(info + kOffModelId);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
    const int n          = g_calls.fetch_add(1) + 1;
    const uint32_t garage = g_api->upgrade_active_garage();
    int fs = 0, rs = 0;
    if (garage) {
        fs = g_api->upgrade_stage(g_mod, g_front, static_cast<int32_t>(garage));
        rs = g_api->upgrade_stage(g_mod, g_rear, static_cast<int32_t>(garage));
        // Only the player's car: the model id the platform recorded for that garage id has to match this car's.
        const int m = fs > 0 ? stored_model(L"car.wheelspacers.front", garage) : rs > 0 ? stored_model(L"car.wheelspacers.rear", garage) : 0;
        if (m > 0 && m != model) fs = rs = 0;
    }
    const float fd = fs * 0.01f * g_scale, rd = rs * 0.01f * g_scale;
    if (fd == 0.0f && rd == 0.0f) {
        if (g_verbose && n <= 60) say("car built: object %p, model %d, driven garage %u: no spacers", car, model, garage);
        return;
    }
    float before[4] = {}, after[4] = {};
    __try {
        float* f = reinterpret_cast<float*>(car + kOffFront);
        std::memcpy(before, f, sizeof before);
        if (g_apply_stock) { f[0] += fd; f[1] += rd; }
        if (g_apply_upgr)  { f[2] += fd; f[3] += rd; }
        std::memcpy(after, f, sizeof after);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
    if (g_verbose)
        say("car built: object %p, model %d, garage %u: front +%d cm, rear +%d cm; offsets %.4f %.4f %.4f %.4f -> %.4f %.4f %.4f %.4f",
            car, model, garage, fs, rs, before[0], before[1], before[2], before[3], after[0], after[1], after[2], after[3]);
}

DWORD WINAPI install_hook(LPVOID) {
    char* game = static_cast<char*>(g_api->wait_for_code(0xFFFFFFFFu));
    if (!game) { say("the game's code never became ready: no hook"); return 0; }
    if (std::memcmp(game + kHookRva, kHookBytes, sizeof kHookBytes) != 0) {
        say("game+0x%X isn't the code this mod was written for (another game build?): not hooked", kHookRva);
        return 0;
    }
    FH3Hook* h = g_api->hook_instruction(g_mod, "Car data built (track offsets)", game + kHookRva, &on_car_built, nullptr);
    if (h && g_api->enable_hooks(g_mod, h)) say("hooked game+0x%X: spacers are added to the track offsets", kHookRva);
    else say("couldn't hook game+0x%X (see the platform's lines above)", kHookRva);
    return 0;
}

int add_upgrade(int side) {   // 0 front, 1 rear
    const wchar_t* axle = side ? L"rear" : L"front";
    g_names[side].resize(kStages);
    g_descs[side].resize(kStages);
    for (int i = 0; i < kStages; ++i) {
        g_names[side][i] = i == 0 ? L"Stock" : L"+" + wnum(i) + L" cm Wheel Spacer";
        g_descs[side][i] = i == 0 ? std::wstring(L"The standard ") + axle + L" track width."
                                  : std::wstring(L"Moves each ") + axle + L" wheel " + wnum(i) + L" cm further out.";
        g_stage_defs[side][i] = {g_names[side][i].c_str(), g_descs[side][i].c_str(), kPrice, nullptr, nullptr};
    }
    FH3UpgradeDef d{};
    d.size           = sizeof d;
    d.id             = side ? "wheelspacers.rear" : "wheelspacers.front";
    d.name           = side ? L"Rear Spacers" : L"Front Spacers";
    d.description    = side ? L"Wheel spacers for the rear axle: a wider rear track."
                            : L"Wheel spacers for the front axle: a wider front track.";
    d.area           = 4;   // Tires and rims
    d.stage_count    = kStages;
    d.stages         = g_stage_defs[side];
    d.on_stage       = on_stage;
    d.user           = side ? &g_rear : nullptr;
    d.preferred_slot = -1;
    return g_api->register_upgrade(g_mod, &d);
}

}  // namespace

FH3MOD_EXPORT int fh3mod_init(const FH3ModApi* api) {
    if (api->api_version < 11) return 1;   // register_upgrade (11) and hook_instruction (7)
    g_api = api;

    FH3ModInfo info{};
    info.size             = sizeof info;
    info.name             = "Universal Wheel Spacers";
    info.version          = WHEELSPACERS_VERSION;
    info.github_repo      = nullptr;
    info.platform_version = FH3MOD_PLATFORM_VERSION;
    info.check_updates    = 0;
    info.description      = "Front and rear wheel spacers for every car: +1 to +9 cm per wheel from Garage > Upgrades > Tires and Rims.";
    info.offline_only     = 1;
    g_mod                 = api->register_mod(&info);
    if (!g_mod) return 1;

    g_cfg_path     = std::wstring(api->azimuth_dir) + L"\\fh3-mods\\WheelSpacers.ini";
    g_platform_ini = std::wstring(api->azimuth_dir) + L"\\fh3-mods\\platform-upgrades.ini";
    load_config();

    g_front = add_upgrade(0);
    g_rear  = add_upgrade(1);
    say("registered: front handle %d, rear handle %d (scale %.2f, stock offsets %d, upgrade offsets %d)", g_front, g_rear,
        g_scale, g_apply_stock, g_apply_upgr);
    if (g_front < 0 || g_rear < 0) return 1;

    if (HANDLE t = CreateThread(nullptr, 0, install_hook, nullptr, 0, nullptr)) CloseHandle(t);
    return 0;
}
