// Universal Wheel Spacers for Forza Horizon 3 (FH3 Mod Platform mod).
//
// Adds two upgrades under Garage > Upgrades > Tires and Rims: "Front Spacers" and "Rear Spacers".
// Stage 0 is stock (fitted on every car), the other stages are +1 cm ... +9 cm per wheel for 300 CR each.
// The platform's upgrade bridge runs the shop and keeps each car's stage.
//
// How the track gets wider: the game's central car loader (game+0x19E6A80) reads Data_CarBody.ModelFrontTrackOuter /
// ModelRearTrackOuter (metres, outer tire edge to outer tire edge) into the car spec at +0xF4 / +0xF8. The mod hooks the
// instruction right after (game+0x19E9027) and adds twice the spacer width. The physics and the model both take the
// track from there, so the wheels move out and the car really is wider. The loader runs for every car the game builds
// (including the shop's car on every cursor move, which is how the stage shows before it is bought).
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

#include "icon_data.h"   // icons/*.png as FH3 .swatchbin (scripts/gen_icons.py)
#include "icon_pack.h"

namespace {

constexpr int kStages          = 10;   // stage 0 (stock) + 1..9 cm: the upgrade bridge allows 10 stages at most
constexpr int kPrice           = 300;  // credits per spacer stage

// Spec hook: the central car loader (game+0x19E6A80: Data_Car, physicsdefinition.bin, Data_CarBody) stores
// ModelFrontTrackOuter / ModelRearTrackOuter at +0xF4 / +0xF8 of the spec object it fills (r14); the Data_Car.Id it was given is saved at [rsp+0x68].
// game+0x19E9027 is the first instruction after both stores (`lea rcx, [rbp+0x640]`).
constexpr uint32_t kSpecRva      = 0x19E9027;
constexpr size_t kOffSpecFront   = 0xF4;
constexpr size_t kOffSpecRear    = 0xF8;
const unsigned char kSpecBytes[] = {0x48, 0x8D, 0x8D, 0x40, 0x06, 0x00, 0x00};

const FH3ModApi* g_api = nullptr;
FH3Mod* g_mod          = nullptr;
int g_front            = -1;
int g_rear             = -1;

// fh3-mods\WheelSpacers.ini (written with these defaults on first start).
float g_scale  = 1.0f;    // multiplies the spacer width
bool g_verbose = false;   // DebugLog=1: log the cars the game builds and what the mod changed (for bug reports)

std::atomic<int> g_calls{0};
std::wstring g_cfg_path, g_platform_ini;

std::vector<std::wstring> g_names[2], g_descs[2];
FH3UpgradeStage g_stage_defs[2][kStages];

void say(const char* fmt, ...) {
    char line[700];
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
    GetPrivateProfileStringW(L"WheelSpacers", L"DebugLog", L"", buf, 64, g_cfg_path.c_str());
    if (buf[0]) g_verbose = std::wcstol(buf, nullptr, 10) != 0;
    else WritePrivateProfileStringW(L"WheelSpacers", L"DebugLog", L"0", g_cfg_path.c_str());
    // keys of the test builds (Log, ApplyBody, ApplySpec, ApplyOffsets, Trace, TraceShopHooks, Register, ApplyStock, ApplyUpgrade) are gone
    for (const wchar_t* old : {L"Log", L"ApplyBody", L"ApplySpec", L"ApplyOffsets", L"Trace", L"TraceShopHooks", L"Register", L"ApplyStock", L"ApplyUpgrade"})
        WritePrivateProfileStringW(L"WheelSpacers", old, nullptr, g_cfg_path.c_str());
}

void on_stage(void* user, uint32_t garage, int stage) {
    if (g_verbose) say("%s spacers: garage %u now has stage %d (+%d cm)", user ? "rear" : "front", garage, stage, stage);
}

// The car being built is the player's when its model matches the one the platform recorded for the driven garage id
// (the platform stores 2154, the game holds Data_Car.Id = 2154002). Sets the fitted front and rear stages (0 for any other car).
void stages_for_model(int model, const char* what, const void* obj, int* fs, int* rs) {
    *fs = *rs = 0;
    const int n           = g_calls.fetch_add(1) + 1;
    uint32_t garage = g_api->upgrade_active_garage();
    if (g_api->api_version >= 14 && g_api->upgrade_stage_for_car) {
        // The platform decides whose car this is (the player's car of that model, not an AI car of the same model in a
        // one-make race, not a loaner) and gives the shop's preview stage; see the platform's docs/car-mods.md.
        *fs = g_api->upgrade_stage_for_car(g_mod, g_front, model);
        *rs = g_api->upgrade_stage_for_car(g_mod, g_rear, model);
        if (g_verbose && n <= 200) say("%s: object %p, model %d: front +%d, rear +%d", what, obj, model, *fs, *rs);
        return;
    }
    if (garage) {
        *fs = g_api->upgrade_stage(g_mod, g_front, static_cast<int32_t>(garage));
        *rs = g_api->upgrade_stage(g_mod, g_rear, static_cast<int32_t>(garage));
        int m = 0;
        if (*fs > 0 || *rs > 0) {
            if (g_api->api_version >= 12 && g_api->upgrade_car_model) m = g_api->upgrade_car_model(static_cast<int32_t>(garage));
            if (m <= 0) m = *fs > 0 ? stored_model(L"car.wheelspacers.front", garage) : stored_model(L"car.wheelspacers.rear", garage);
        }
        if (m > 0 && m != model && m != model / 1000) {
            if (g_verbose && n <= 200) say("%s: object %p, model %d: not the driven car (garage %u is model %d), left alone", what, obj, model, garage, m);
            *fs = *rs = 0;
            return;
        }
    }
    if (g_verbose && n <= 200 && !*fs && !*rs) say("%s: object %p, model %d, driven garage %u: no spacers", what, obj, model, garage);
}

// Adds 2 x the spacer width to two track outer floats (front, rear) of an object; logs the change.
void widen(const char* what, char* base, size_t off_front, size_t off_rear, int model, int fs, int rs) {
    float before[2] = {}, after[2] = {};
    __try {
        float* f = reinterpret_cast<float*>(base + off_front);
        float* r = reinterpret_cast<float*>(base + off_rear);
        before[0] = *f;
        before[1] = *r;
        *f += 2.0f * fs * 0.01f * g_scale;
        *r += 2.0f * rs * 0.01f * g_scale;
        after[0] = *f;
        after[1] = *r;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
    if (g_verbose && g_calls.load() <= 200)
        say("%s: object %p, model %d: front +%d cm, rear +%d cm; track outer front %.4f -> %.4f, rear %.4f -> %.4f", what, base, model, fs, rs,
            before[0], after[0], before[1], after[1]);
}

// Runs every time the central car loader has stored a car's track outer values. r14 is the spec object.
// The Data_Car.Id the loader was given: it saves it at [rsp+0x68] at entry (r12 is reused later in the function); the
// pointer it was passed is the fallback (the home slot of r8, [rbp+0xE70]).
int spec_model(const FH3Context* ctx) {
    __try {
        const int a = *reinterpret_cast<const int*>(ctx->rsp + 0x68);
        if (a >= 100 && a < 100000000) return a;   // Data_Car ids start at 249 (84 cars are below 1000)
        const int* p = *reinterpret_cast<int* const*>(ctx->rbp + 0xE70);
        const int b  = *p;
        if (b >= 100 && b < 100000000) return b;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return 0;
}

void on_spec_built(FH3Context* ctx, void*) {
    char* spec      = reinterpret_cast<char*>(ctx->r14);
    const int model = spec_model(ctx);
    if (!model) {
        if (g_verbose && g_calls.fetch_add(1) < 200) say("spec: object %p: couldn't read the Data_Car id", spec);
        return;
    }
    int fs, rs;
    stages_for_model(model, "spec", spec, &fs, &rs);
    if (fs || rs) widen("spec", spec, kOffSpecFront, kOffSpecRear, model, fs, rs);
}

bool hook_at(char* game, uint32_t rva, const unsigned char* bytes, size_t n, const char* name, FH3InstructionFn fn) {
    if (std::memcmp(game + rva, bytes, n) != 0) {
        say("game+0x%X isn't the code this mod was written for (another game build?): %s not hooked", rva, name);
        return false;
    }
    FH3Hook* h = g_api->hook_instruction(g_mod, name, game + rva, fn, nullptr);
    if (h && g_api->enable_hooks(g_mod, h)) {
        if (g_verbose) say("hooked game+0x%X (%s)", rva, name);
        return true;
    }
    say("couldn't hook game+0x%X (%s); see the platform's lines above", rva, name);
    return false;
}

DWORD WINAPI install_hooks(LPVOID) {
    char* game = static_cast<char*>(g_api->wait_for_code(0xFFFFFFFFu));
    if (!game) { say("the game's code never became ready: no hooks"); return 0; }
    hook_at(game, kSpecRva, kSpecBytes, sizeof kSpecBytes, "Car spec loaded (track outer)", &on_spec_built);
    return 0;
}

// The icons in icons/ are handed to the game as Upgrade_Parts\Custom\uws_<slot>_Icon.swatchbin (platform set_file).
const char kIconDir[]   = "GAME:\\Media\\UI\\Textures\\Data_Bound\\Upgrade_Parts\\";
const char kLargeIcon[] = "GAME:\\Media\\UI\\Textures\\Data_Bound\\Upgrade_Parts\\Tire_Rims\\tires_rims_LG_Icon.swatchbin";
std::string g_icon_paths[3];   // front, rear, spacer

void install_icons() {
    if (g_api->api_version < 2 || !g_api->set_file) return;
    static const char base[] = "ui/textures/data_bound/Upgrade_Parts/";
    int ok = 0, total = 0;
    for (const IconEntry* e = k_icons; e->slot; e++) {
        char path[200];
        int n = 0;
        for (int i = 0; base[i]; i++) path[n++] = base[i];
        icon_name(e, path + n, '/');
        const unsigned size = icon_unpack(e, g_icon_buf);
        ++total;
        if (g_api->set_file(g_mod, path, g_icon_buf, size)) ++ok;
    }
    if (g_verbose) say("icons handed to the game: %d/%d", ok, total);
}

// The game path of one of our icons, or nullptr (the platform then uses the game's own).
const char* icon_path(int which, const char* slot) {
    if (!icon_has(slot, 0)) return nullptr;
    IconEntry e{slot, 0, nullptr, 0};
    char nm[160];
    icon_name(&e, nm, '\\');
    g_icon_paths[which] = std::string(kIconDir) + nm;
    return g_icon_paths[which].c_str();
}

int add_upgrade(int side) {   // 0 front, 1 rear
    const wchar_t* axle = side ? L"rear" : L"front";
    const char* stage_icon = icon_path(2, "spacer");
    g_names[side].resize(kStages);
    g_descs[side].resize(kStages);
    for (int i = 0; i < kStages; ++i) {
        g_names[side][i] = i == 0 ? L"Stock" : L"+" + wnum(i) + L" cm Wheel Spacer";
        g_descs[side][i] = i == 0 ? std::wstring(L"The standard ") + axle + L" track width."
                                  : std::wstring(L"Moves each ") + axle + L" wheel " + wnum(i) + L" cm further out.";
        g_stage_defs[side][i] = {g_names[side][i].c_str(), g_descs[side][i].c_str(), kPrice, i == 0 ? nullptr : stage_icon,
                                 i == 0 ? nullptr : (stage_icon ? kLargeIcon : nullptr)};
    }
    FH3UpgradeDef d{};
    d.size           = sizeof d;
    d.id             = side ? "wheelspacers.rear" : "wheelspacers.front";
    d.name           = side ? L"Rear Spacers" : L"Front Spacers";
    d.description    = side ? L"Wheel spacers for the rear axle: a wider rear track."
                            : L"Wheel spacers for the front axle: a wider front track.";
    d.area           = 4;   // Tires and rims
    d.icon           = icon_path(side, side ? "rear" : "front");
    d.icon_large     = d.icon ? kLargeIcon : nullptr;
    d.stage_count    = kStages;
    d.stages         = g_stage_defs[side];
    d.on_stage       = on_stage;
    d.user           = side ? &g_rear : nullptr;
    d.preferred_slot = -1;
    d.camera_part    = side ? 11 : 10;   // the shop's camera of rear / front rim size: the wheel of that axle
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
    info.github_repo      = "Levilente/UniversalWheelSpacers_FH3";
    info.platform_version = FH3MOD_PLATFORM_VERSION;
    info.check_updates    = 1;
    info.description      = "Front and rear wheel spacers for every car: +1 to +9 cm per wheel from Garage > Upgrades > Tires and Rims.";
    info.offline_only     = 1;
    g_mod                 = api->register_mod(&info);
    if (!g_mod) return 1;

    g_cfg_path     = std::wstring(api->azimuth_dir) + L"\\fh3-mods\\WheelSpacers.ini";
    g_platform_ini = std::wstring(api->azimuth_dir) + L"\\fh3-mods\\platform-upgrades.ini";
    load_config();

    install_icons();
    g_front = add_upgrade(0);
    g_rear  = add_upgrade(1);
    if (g_verbose) say("registered: front handle %d, rear handle %d (scale %.2f)", g_front, g_rear, g_scale);
    if (g_front < 0 || g_rear < 0) return 1;

    if (HANDLE t = CreateThread(nullptr, 0, install_hooks, nullptr, 0, nullptr)) CloseHandle(t);
    return 0;
}
