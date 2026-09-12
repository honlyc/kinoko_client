#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>

#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cwchar>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "winmm.lib")

namespace {

constexpr uintptr_t ImageBase = 0x400000;

constexpr uintptr_t WvsContextInstanceVa = 0xC64068;
constexpr uintptr_t WvsContextGetItemCountVa = 0x9F3E10;
constexpr uintptr_t WorldMapMouseButtonVa = 0x9BF1C0;
constexpr uintptr_t WorldMapCheckSpotInfoVa = 0x9B59A0;
constexpr uintptr_t COutPacketCtorVa = 0x68D090;
constexpr uintptr_t COutPacketEncode4Va = 0x4153B0;
constexpr uintptr_t COutPacketDtorVa = 0x42AAE0;
constexpr uintptr_t SendPacketVa = 0x429B80;
constexpr uintptr_t ClientSocketInstanceVa = 0xC64064;

constexpr uint8_t ExpectedWorldMapMouseButton[] = {
    0x56, 0x8B, 0xF1, 0x8B, 0x86, 0x10, 0x0B, 0x00, 0x00
};

constexpr int HyperTeleportRockItemId = 5040001;
constexpr int HyperTeleportRockRequestOpcode = 0x105;

constexpr int WorldMapSelectedIndexOffset = 0xAF0;
constexpr int WorldMapSpotArrayOffset = 0xB00;
constexpr int WorldMapSpotStride = 0x90;
constexpr int WorldMapSpotFieldArrayOffset = 0x50;
constexpr int WorldMapSpotXOffset = 0x00;
constexpr int WorldMapSpotYOffset = 0x04;
constexpr int WorldMapClickOriginX = 13;
constexpr int WorldMapClickOriginY = 24;
constexpr int WorldMapFallbackHitRadius = 18;

constexpr UINT WmLButtonDown = 0x0201;
constexpr UINT WmLButtonUp = 0x0202;
constexpr UINT WmLButtonDoubleClick = 0x0203;
constexpr DWORD StartupHookDelayMs = 12000;
constexpr DWORD SendCooldownMs = 500;

struct Detour {
    uint8_t* target = nullptr;
    uint8_t original[16]{};
    size_t length = 0;
    void* trampoline = nullptr;
};

using WorldMapMouseButtonFn = void(__thiscall*)(void*, unsigned int, unsigned int, long, long);
using WorldMapCheckSpotInfoFn = int(__thiscall*)(void*, long, long);
using WvsContextGetItemCountFn = int(__thiscall*)(void*, int);
using COutPacketCtorFn = void*(__thiscall*)(void*, int);
using COutPacketEncode4Fn = void(__thiscall*)(void*, int);
using COutPacketDtorFn = void(__thiscall*)(void*);
using SendPacketFn = void(__cdecl*)(void*);

HMODULE g_module = nullptr;
Detour g_worldMapMouseButtonDetour{};
WorldMapMouseButtonFn g_originalWorldMapMouseButton = nullptr;
DWORD g_lastSendTick = 0;

uintptr_t ToAddress(uintptr_t va)
{
    return reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)) + (va - ImageBase);
}

void Log(const char* format, ...)
{
    wchar_t modulePath[MAX_PATH]{};
    GetModuleFileNameW(g_module, modulePath, MAX_PATH);
    wchar_t* slash = wcsrchr(modulePath, L'\\');
    if (slash) {
        slash[1] = L'\0';
    }
    wcscat_s(modulePath, L"hyper_teleport_rock.log");

    FILE* file = nullptr;
    if (_wfopen_s(&file, modulePath, L"a") != 0 || !file) {
        return;
    }

    va_list args;
    va_start(args, format);
    vfprintf(file, format, args);
    va_end(args);
    fputc('\n', file);
    fclose(file);
}

bool InstallDetour(Detour& detour, uintptr_t va, void* hook, size_t length,
                   const uint8_t* expectedBytes, size_t expectedLength,
                   const char* name)
{
    if (length > sizeof(detour.original) || expectedLength != length) {
        Log("%s detour rejected: invalid length", name);
        return false;
    }

    detour.target = reinterpret_cast<uint8_t*>(ToAddress(va));
    detour.length = length;

    if (std::memcmp(detour.target, expectedBytes, expectedLength) != 0) {
        Log("%s detour rejected: v95 signature mismatch at 0x%08X",
            name, static_cast<unsigned>(va));
        return false;
    }

    std::memcpy(detour.original, detour.target, length);

    auto* trampoline = reinterpret_cast<uint8_t*>(VirtualAlloc(
        nullptr,
        length + 5,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE));
    if (!trampoline) {
        Log("%s detour rejected: no trampoline", name);
        return false;
    }

    std::memcpy(trampoline, detour.original, length);
    trampoline[length] = 0xE9;
    *reinterpret_cast<int32_t*>(trampoline + length + 1) =
        static_cast<int32_t>((detour.target + length) - (trampoline + length + 5));
    detour.trampoline = trampoline;

    DWORD oldProtect = 0;
    if (!VirtualProtect(detour.target, length, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        detour.trampoline = nullptr;
        Log("%s detour rejected: VirtualProtect failed", name);
        return false;
    }

    detour.target[0] = 0xE9;
    *reinterpret_cast<int32_t*>(detour.target + 1) =
        static_cast<int32_t>(reinterpret_cast<uint8_t*>(hook) - (detour.target + 5));
    for (size_t i = 5; i < length; ++i) {
        detour.target[i] = 0x90;
    }

    DWORD ignored = 0;
    VirtualProtect(detour.target, length, oldProtect, &ignored);
    FlushInstructionCache(GetCurrentProcess(), detour.target, length);
    Log("%s detour installed at 0x%08X", name, static_cast<unsigned>(va));
    return true;
}

void RemoveDetour(Detour& detour)
{
    if (detour.target && detour.length) {
        DWORD oldProtect = 0;
        if (VirtualProtect(detour.target, detour.length, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            std::memcpy(detour.target, detour.original, detour.length);
            DWORD ignored = 0;
            VirtualProtect(detour.target, detour.length, oldProtect, &ignored);
            FlushInstructionCache(GetCurrentProcess(), detour.target, detour.length);
        }
    }

    if (detour.trampoline) {
        VirtualFree(detour.trampoline, 0, MEM_RELEASE);
    }

    detour = {};
}

bool IsPlausibleMapId(int mapId)
{
    return mapId > 0 && mapId < 1000000000;
}

bool TryReadTargetFromSpot(uint8_t* spot, int& targetMapId)
{
    targetMapId = 0;
    if (!spot) {
        return false;
    }

    auto* fields = *reinterpret_cast<int**>(spot + WorldMapSpotFieldArrayOffset);
    if (!fields) {
        return false;
    }

    int fieldCount = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(fields) - sizeof(int));
    if (fieldCount <= 0 || fieldCount > 10000) {
        return false;
    }

    int candidate = fields[0];
    if (!IsPlausibleMapId(candidate)) {
        return false;
    }

    targetMapId = candidate;
    return true;
}

bool TryResolveWorldMapTarget(void* mouseHandlerSelf, long x, long y, int& targetMapId)
{
    targetMapId = 0;
    if (!mouseHandlerSelf) {
        return false;
    }

    __try {
        auto* worldMap = reinterpret_cast<uint8_t*>(mouseHandlerSelf) - 4;
        auto checkSpotInfo = reinterpret_cast<WorldMapCheckSpotInfoFn>(ToAddress(WorldMapCheckSpotInfoVa));
        checkSpotInfo(worldMap, x, y);

        auto* spots = *reinterpret_cast<uint8_t**>(worldMap + WorldMapSpotArrayOffset);
        if (!spots) {
            return false;
        }

        int spotCount = *reinterpret_cast<int*>(spots - sizeof(int));
        if (spotCount <= 0 || spotCount > 4096) {
            return false;
        }

        int selected = *reinterpret_cast<int*>(worldMap + WorldMapSelectedIndexOffset);
        if (selected >= 0 && selected < spotCount) {
            auto* spot = spots + selected * WorldMapSpotStride;
            if (TryReadTargetFromSpot(spot, targetMapId)) {
                return true;
            }
        }

        const long clickX = x - WorldMapClickOriginX;
        const long clickY = y - WorldMapClickOriginY;
        const long maxDistance = WorldMapFallbackHitRadius * WorldMapFallbackHitRadius;
        long bestDistance = maxDistance + 1;
        int bestTarget = 0;

        for (int i = 0; i < spotCount; ++i) {
            auto* spot = spots + i * WorldMapSpotStride;
            int spotX = *reinterpret_cast<int*>(spot + WorldMapSpotXOffset);
            int spotY = *reinterpret_cast<int*>(spot + WorldMapSpotYOffset);
            long dx = spotX - clickX;
            long dy = spotY - clickY;
            long distance = dx * dx + dy * dy;
            if (distance > maxDistance || distance >= bestDistance) {
                continue;
            }

            int candidate = 0;
            if (!TryReadTargetFromSpot(spot, candidate)) {
                continue;
            }

            bestDistance = distance;
            bestTarget = candidate;
        }

        if (bestTarget) {
            targetMapId = bestTarget;
            return true;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }

    return false;
}

bool HasHyperTeleportRock()
{
    __try {
        auto* context = *reinterpret_cast<void**>(ToAddress(WvsContextInstanceVa));
        if (!context) {
            return false;
        }

        auto getItemCount = reinterpret_cast<WvsContextGetItemCountFn>(ToAddress(WvsContextGetItemCountVa));
        return getItemCount(context, HyperTeleportRockItemId) > 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void SendWorldMapTransfer(int targetMapId)
{
    if (!IsPlausibleMapId(targetMapId)) {
        return;
    }

    DWORD now = timeGetTime();
    if (now - g_lastSendTick < SendCooldownMs) {
        return;
    }

    __try {
        auto* socket = *reinterpret_cast<void**>(ToAddress(ClientSocketInstanceVa));
        if (!socket) {
            return;
        }

        uint8_t packet[32]{};
        auto construct = reinterpret_cast<COutPacketCtorFn>(ToAddress(COutPacketCtorVa));
        auto encode4 = reinterpret_cast<COutPacketEncode4Fn>(ToAddress(COutPacketEncode4Va));
        auto sendPacket = reinterpret_cast<SendPacketFn>(ToAddress(SendPacketVa));
        auto destroy = reinterpret_cast<COutPacketDtorFn>(ToAddress(COutPacketDtorVa));

        construct(packet, HyperTeleportRockRequestOpcode);
        encode4(packet, targetMapId);
        sendPacket(packet);
        destroy(packet);
        g_lastSendTick = now;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

void __fastcall HookWorldMapMouseButton(void* self, void*, unsigned int message,
                                        unsigned int param, long x, long y)
{
    if (message == WmLButtonDown || message == WmLButtonUp || message == WmLButtonDoubleClick) {
        int targetMapId = 0;
        if (TryResolveWorldMapTarget(self, x, y, targetMapId) && HasHyperTeleportRock()) {
            SendWorldMapTransfer(targetMapId);
            return;
        }
    }

    if (!g_originalWorldMapMouseButton && g_worldMapMouseButtonDetour.trampoline) {
        g_originalWorldMapMouseButton =
            reinterpret_cast<WorldMapMouseButtonFn>(g_worldMapMouseButtonDetour.trampoline);
    }

    if (g_originalWorldMapMouseButton) {
        g_originalWorldMapMouseButton(self, message, param, x, y);
    }
}

DWORD WINAPI InitThread(void*)
{
    Sleep(StartupHookDelayMs);

    InstallDetour(
        g_worldMapMouseButtonDetour,
        WorldMapMouseButtonVa,
        reinterpret_cast<void*>(HookWorldMapMouseButton),
        sizeof(ExpectedWorldMapMouseButton),
        ExpectedWorldMapMouseButton,
        sizeof(ExpectedWorldMapMouseButton),
        "CWorldMapDlg::OnMouseButton");

    return 0;
}

} // namespace

void InitHyperTeleportRock(HMODULE module)
{
    g_module = module;

    HANDLE thread = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
    if (thread) {
        CloseHandle(thread);
    }
}

void ShutdownHyperTeleportRock()
{
    RemoveDetour(g_worldMapMouseButtonDetour);
}
