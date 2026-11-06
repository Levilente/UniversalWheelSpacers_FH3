// Universal Wheel Spacers for Forza Horizon 3 (FH3 Mod Platform mod).
//
// Adds two upgrades under Garage > Upgrades > Tires and Rims: "Front Spacers" and "Rear Spacers".
// Stage 0 is stock (fitted on every car by default), the other stages are +1 cm, +2 cm, ... per wheel
// for 300 CR each. The stage is stored per owned car (garage id) by the platform's upgrade bridge.
//
// How the track width is changed: the game builds a car's data object from the database. The function that loads
// the car body (Data_CarBody) stores ModelFrontTrackOuter / ModelRearTrackOuter (metres, outer edge of one tire to
// the outer edge of the other) at +0x234 / +0x238 of the object and then hands the body data to the car's physics
// (game+0x18BADED is the first instruction after both stores). This mod adds twice the spacer width there, which is
// also what makes a widebody kit's wheels jump out. A second, older approach (the four CarTrackOffsets values stored by
// game+0x189F020) is still here, off by default, because the game doesn't seem to use those values for the wheels.
//
// Status: written from reading the game's code; the log lines show every step.
#include "fh3mod.h"

#include <windows.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

namespace {

constexpr int kStages          = 10;   // stage 0 (stock) + 1..9 cm: the upgrade bridge allows 10 stages at most
constexpr int kPrice           = 300;  // credits per spacer stage
constexpr size_t kOffModelInfo = 0x110;  // car -> record whose +0x30 is the car's model id (Data_Car.Id)
constexpr size_t kOffModelId   = 0x30;

// Body hook: `mov rcx, [rsi+0x71E8]`, right after ModelFrontTrackOuter / ModelRearTrackOuter are stored.
constexpr uint32_t kBodyRva      = 0x18BADED;
constexpr size_t kOffBodyFront   = 0x234;
constexpr size_t kOffBodyRear    = 0x238;
const unsigned char kBodyBytes[] = {0x48, 0x8B, 0x8E, 0xE8, 0x71, 0x00, 0x00};

// Offsets hook: `mov rcx, [rdi+0x71E8]`, right after FrontOffset, RearOffset, UpgradeFrontOffset, UpgradeRearOffset.
constexpr uint32_t kOffsetsRva      = 0x189F8B8;
constexpr size_t kOffOffsets        = 0x773C;
const unsigned char kOffsetsBytes[] = {0x48, 0x8B, 0x8F, 0xE8, 0x71, 0x00, 0x00};

const FH3ModApi* g_api = nullptr;
FH3Mod* g_mod          = nullptr;
int g_front            = -1;
int g_rear             = -1;

// fh3-mods\WheelSpacers.ini (written with these defaults on first start).
float g_scale       = 1.0f;   // multiplies the spacer width; -1 turns the wheels the other way if they move inwards
bool g_apply_body   = true;   // add 2 x width to ModelFrontTrackOuter / ModelRearTrackOuter
bool g_apply_offs   = false;  // add the width to the four CarTrackOffsets values as well
bool g_verbose      = true;   // log every car the game builds (the first 80) and every change

std::atomic<int> g_calls{0};
std::wstring g_cfg_path, g_platform_ini;

std::vector<std::wstring> g_names[2], g_descs[2];
FH3UpgradeStage g_stage_defs[2][kStages];

void say(const char* fmt, ...) {
    char line[320];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    if (g_api && g_mod) g_api->log(g_mod, line);
}

std::wstring wnum(long long v) { return std::to_wstring(v); }

// The platform keeps "stage|owned mask|model number" per garage id in its own ini; the model number is what lets this
// mod tell the player's car from other cars the game builds.
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
    if (buf[0]) g_scale = static_cast<float>(std::wcstod(buf, nullptr));
    else WritePrivateProfileStringW(L"WheelSpacers", L"Scale", L"1.0", g_cfg_path.c_str());
    const struct { const wchar_t* key; bool* value; int def; } flags[] = {
        {L"ApplyBody", &g_apply_body, 1}, {L"ApplyOffsets", &g_apply_offs, 0}, {L"Log", &g_verbose, 1}};
    for (const auto& f : flags) {
        GetPrivateProfileStringW(L"WheelSpacers", f.key, L"", buf, 64, g_cfg_path.c_str());
        if (buf[0]) *f.value = std::wcstol(buf, nullptr, 10) != 0;
        else WritePrivateProfileStringW(L"WheelSpacers", f.key, f.def ? L"1" : L"0", g_cfg_path.c_str());
    }
}

void on_stage(void* user, uint32_t garage, int stage) {
    if (g_verbose) say("%s spacers: garage %u now has stage %d (+%d cm)", user ? "rear" : "front", garage, stage, stage);
}

// The car being built is the player's when its model matches the one the platform recorded for the driven garage id
// (the platform stores 2154, the game's car data holds Data_Car.Id = 2154002). Returns the car's model id, 0 if the
// object couldn't be read; sets the fitted front and rear stages (0 for any other car).
int driven_stages(const char* car, const char* what, int* fs, int* rs) {
    int model = 0;
    __try {
        const char* info = *reinterpret_cast<char* const*>(car + kOffModelInfo);
        model            = *reinterpret_cast<const int*>(info + kOffModelId);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    *fs = *rs = 0;
    const int n           = g_calls.fetch_add(1) + 1;
    const uint32_t garage = g_api->upgrade_active_garage();
    if (garage) {
        *fs = g_api->upgrade_stage(g_mod, g_front, static_cast<int32_t>(garage));
        *rs = g_api->upgrade_stage(g_mod, g_rear, static_cast<int32_t>(garage));
        const int m = *fs > 0 ? stored_model(L"car.wheelspacers.front", garage) : *rs > 0 ? stored_model(L"car.wheelspacers.rear", garage) : 0;
        if (m > 0 && m != model && m != model / 1000) {
            if (g_verbose && n <= 80) say("%s: object %p, model %d: not the driven car (garage %u is model %d), left alone", what, car, model, garage, m);
            *fs = *rs = 0;
        } else if (g_verbose && n <= 80 && !*fs && !*rs) {
            say("%s: object %p, model %d, driven garage %u: no spacers", what, car, model, garage);
        }
    }
    return model;
}

// Runs on the game's thread every time the game loads a car's body data. rsi is the car object.
void on_body_built(FH3Context* ctx, void*) {
    const char* car = reinterpret_cast<const char*>(ctx->rsi);
    int fs, rs;
    const int model = driven_stages(car, "body", &fs, &rs);
    if (!model || (!fs && !rs)) return;
    if (!g_apply_body) return;
    float before[2] = {}, after[2] = {};
    __try {
        float* f = reinterpret_cast<float*>(const_cast<char*>(car) + kOffBodyFront);
        float* r = reinterpret_cast<float*>(const_cast<char*>(car) + kOffBodyRear);
        before[0] = *f;
        before[1] = *r;
        *f += 2.0f * fs * 0.01f * g_scale;
        *r += 2.0f * rs * 0.01f * g_scale;
        after[0] = *f;
        after[1] = *r;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
    if (g_verbose)
        say("body: object %p, model %d: front +%d cm, rear +%d cm; track outer front %.4f -> %.4f, rear %.4f -> %.4f", car, model, fs, rs,
            before[0], after[0], before[1], after[1]);
}

// Runs every time the game stores a car's four CarTrackOffsets values. rdi is the car object.
void on_offsets_built(FH3Context* ctx, void*) {
    if (!g_apply_offs) return;
    const char* car = reinterpret_cast<const char*>(ctx->rdi);
    int fs, rs;
    const int model = driven_stages(car, "offsets", &fs, &rs);
    if (!model || (!fs && !rs)) return;
    const float fd = fs * 0.01f * g_scale, rd = rs * 0.01f * g_scale;
    __try {
        float* f = reinterpret_cast<float*>(const_cast<char*>(car) + kOffOffsets);
        f[0] += fd; f[1] += rd; f[2] += fd; f[3] += rd;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
    if (g_verbose) say("offsets: object %p, model %d: front +%d cm, rear +%d cm added to the four CarTrackOffsets values", car, model, fs, rs);
}

bool hook_at(char* game, uint32_t rva, const unsigned char* bytes, size_t n, const char* name, FH3InstructionFn fn) {
    if (std::memcmp(game + rva, bytes, n) != 0) {
        say("game+0x%X isn't the code this mod was written for (another game build?): %s not hooked", rva, name);
        return false;
    }
    FH3Hook* h = g_api->hook_instruction(g_mod, name, game + rva, fn, nullptr);
    if (h && g_api->enable_hooks(g_mod, h)) {
        say("hooked game+0x%X (%s)", rva, name);
        return true;
    }
    say("couldn't hook game+0x%X (%s); see the platform's lines above", rva, name);
    return false;
}

DWORD WINAPI install_hooks(LPVOID) {
    char* game = static_cast<char*>(g_api->wait_for_code(0xFFFFFFFFu));
    if (!game) { say("the game's code never became ready: no hooks"); return 0; }
    hook_at(game, kBodyRva, kBodyBytes, sizeof kBodyBytes, "Car body built (track outer)", &on_body_built);
    hook_at(game, kOffsetsRva, kOffsetsBytes, sizeof kOffsetsBytes, "Car data built (track offsets)", &on_offsets_built);
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
    say("registered: front handle %d, rear handle %d (scale %.2f, body %d, offsets %d)", g_front, g_rear, g_scale, g_apply_body, g_apply_offs);
    if (g_front < 0 || g_rear < 0) return 1;

    if (HANDLE t = CreateThread(nullptr, 0, install_hooks, nullptr, 0, nullptr)) CloseHandle(t);
    return 0;
}
