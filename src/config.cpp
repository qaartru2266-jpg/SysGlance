#include "config.h"

#include <shlobj.h>

#include <filesystem>

namespace sysglance {
namespace {

constexpr wchar_t kSectionGeneral[] = L"General";
constexpr wchar_t kSectionDisplay[] = L"Display";
constexpr wchar_t kSectionHud[] = L"HUD";
constexpr wchar_t kSectionHudLastGood[] = L"HUDLastGood";

std::wstring LocalAppData() {
    wchar_t path[MAX_PATH]{};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT,
                                   path))) {
        return path;
    }
    return L".";
}

bool WriteBool(const std::wstring& path, const wchar_t* section, const wchar_t* key, bool value) {
    return WritePrivateProfileStringW(section, key, value ? L"1" : L"0", path.c_str()) != FALSE;
}

}  // namespace

std::wstring ConfigService::ConfigDirectory() const {
    return LocalAppData() + L"\\SysGlance";
}

std::wstring ConfigService::Path() const {
    return ConfigDirectory() + L"\\config.ini";
}

int ConfigService::ReadInt(const std::wstring& path, const wchar_t* section,
                           const wchar_t* key, int fallback) {
    return GetPrivateProfileIntW(section, key, fallback, path.c_str());
}

bool ConfigService::ReadBool(const std::wstring& path, const wchar_t* section,
                             const wchar_t* key, bool fallback) {
    const int value = ReadInt(path, section, key, fallback ? 1 : 0);
    return value != 0;
}

std::uint64_t ConfigService::ReadUInt64(const std::wstring& path, const wchar_t* section,
                                        const wchar_t* key, std::uint64_t fallback) {
    wchar_t value[64]{};
    const auto fallbackText = std::to_wstring(fallback);
    GetPrivateProfileStringW(section, key, fallbackText.c_str(), value,
                             static_cast<DWORD>(std::size(value)), path.c_str());
    wchar_t* end = nullptr;
    const auto parsed = _wcstoui64(value, &end, 0);
    return end == value ? fallback : parsed;
}

AppConfig ConfigService::Load() const {
    AppConfig config;
    const auto path = Path();

    const auto mode = ReadInt(path, kSectionGeneral, L"DisplayMode", 0);
    config.displayMode = mode == 1 ? DisplayMode::Taskbar : mode == 2 ? DisplayMode::Hud
                                                                       : DisplayMode::Tray;
    config.refreshIntervalMs = ReadInt(path, kSectionGeneral, L"RefreshIntervalMs", 1000);
    config.fontSize = ReadInt(path, kSectionDisplay, L"FontSize", 12);
    const int hudLayoutVersion = ReadInt(path, kSectionHud, L"LayoutVersion", 0);
    if (hudLayoutVersion < 2 && config.fontSize == 10) {
        config.fontSize = 12;
    }

    config.showCpu = ReadBool(path, kSectionDisplay, L"ShowCpu", true);
    config.showMemory = ReadBool(path, kSectionDisplay, L"ShowMemory", true);
    config.memoryShowPercent = ReadBool(path, kSectionDisplay, L"MemoryShowPercent", false);
    config.gpuMemoryShowPercent = ReadBool(path, kSectionDisplay, L"GpuMemoryShowPercent", false);
    config.showGpu = ReadBool(path, kSectionDisplay, L"ShowGpu", false);
    config.showNetwork = ReadBool(path, kSectionDisplay, L"ShowNetwork", true);
    config.showPercentDecimal = ReadBool(path, kSectionDisplay, L"ShowPercentDecimal", false);
    config.showNetworkArrows = ReadBool(path, kSectionDisplay, L"ShowNetworkArrows", false);
    config.includeVirtualNetworkInterfaces =
        ReadBool(path, kSectionDisplay, L"IncludeVirtualNetworkInterfaces", false);
    config.selectedNetworkLuid = ReadUInt64(path, kSectionDisplay, L"SelectedNetworkLuid", 0);
    config.selectedGpuLuid = ReadUInt64(path, kSectionDisplay, L"SelectedGpuLuid", 0);
    config.darkTheme = ReadBool(path, kSectionDisplay, L"DarkTheme", true);

    config.hudLocked = ReadBool(path, kSectionHud, L"Locked", false);
    config.hudClickThrough = ReadBool(path, kSectionHud, L"ClickThrough", false);
    if (config.hudClickThrough) {
        config.hudLocked = true;
    }
    // Network-only mode was formerly toggled by a HUD double-click. HUD left
    // clicks are intentionally inert now, so migrate any legacy saved state
    // back to the complete layout rather than leaving users stuck in a mode
    // they can no longer toggle from the UI.
    config.hudNetworkOnly = false;
    config.hudOpacity = ReadInt(path, kSectionHud, L"Opacity", 90);
    config.hudWidthDip = ReadInt(path, kSectionHud, L"WidthDip", 360);
    config.hudHeightDip = ReadInt(path, kSectionHud, L"HeightDip", 34);
    config.hudBorderColor = static_cast<COLORREF>(
        ReadInt(path, kSectionHud, L"BorderColor", static_cast<int>(RGB(255, 96, 0))));
    config.hudTextColor = static_cast<COLORREF>(
        ReadInt(path, kSectionHud, L"TextColor", static_cast<int>(RGB(255, 255, 255))));
    config.hudBackgroundColor = static_cast<COLORREF>(
        ReadInt(path, kSectionHud, L"BackgroundColor", static_cast<int>(RGB(10, 13, 18))));
    config.hudBorderThicknessTenths = ReadInt(path, kSectionHud, L"BorderThicknessTenths", 5);
    config.hudColorPreset = ReadInt(path, kSectionHud, L"ColorPreset", 0);
    config.hudRect.left = ReadInt(path, kSectionHud, L"Left", 0);
    config.hudRect.top = ReadInt(path, kSectionHud, L"Top", 0);
    config.hudRect.right = ReadInt(path, kSectionHud, L"Right", 0);
    config.hudRect.bottom = ReadInt(path, kSectionHud, L"Bottom", 0);

    config.autoStart = ReadBool(path, kSectionGeneral, L"AutoStart", false);
    Normalize(config);
    return config;
}

void ConfigService::Normalize(AppConfig& config) const {
    if (config.displayMode != DisplayMode::Tray && config.displayMode != DisplayMode::Taskbar &&
        config.displayMode != DisplayMode::Hud) {
        config.displayMode = DisplayMode::Tray;
    }
    if (!IsKnownRefreshInterval(config.refreshIntervalMs)) config.refreshIntervalMs = 1000;
    config.fontSize = std::clamp(config.fontSize, 1, 1000);
    config.hudOpacity = std::clamp(config.hudOpacity, 30, 100);
    config.hudWidthDip = std::clamp(config.hudWidthDip, 1, 32000);
    config.hudHeightDip = std::clamp(config.hudHeightDip, 1, 32000);
    config.hudBorderThicknessTenths = std::clamp(config.hudBorderThicknessTenths, 1, 1000);
    config.hudColorPreset = std::clamp(config.hudColorPreset, -1, 2);
    if (config.hudClickThrough) config.hudLocked = true;
}

AppConfig ConfigService::RecommendedHud(const AppConfig& base) const {
    AppConfig result = base;
    result.hudOpacity = 90;
    result.hudWidthDip = 360;
    result.hudHeightDip = 34;
    result.fontSize = 12;
    result.hudBorderThicknessTenths = 5;
    result.hudBorderColor = RGB(255, 96, 0);
    result.hudTextColor = RGB(255, 255, 255);
    result.hudBackgroundColor = RGB(10, 13, 18);
    result.hudColorPreset = 0;
    result.hudLocked = false;
    result.hudClickThrough = false;
    result.hudNetworkOnly = false;
    result.showGpu = false;
    result.showPercentDecimal = false;
    result.showNetworkArrows = false;
    return result;
}

bool ConfigService::Save(const AppConfig& config) const {
    const auto directory = ConfigDirectory();
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) {
        return false;
    }

    const auto path = Path();
    bool saved = true;
    const auto write = [&](const wchar_t* section, const wchar_t* key, const std::wstring& value) {
        saved = WritePrivateProfileStringW(section, key, value.c_str(), path.c_str()) != FALSE && saved;
    };
    const auto writeBool = [&](const wchar_t* section, const wchar_t* key, bool value) {
        saved = WriteBool(path, section, key, value) && saved;
    };

    write(kSectionGeneral, L"DisplayMode", std::to_wstring(static_cast<int>(config.displayMode)));
    write(kSectionGeneral, L"RefreshIntervalMs", std::to_wstring(config.refreshIntervalMs));
    writeBool(kSectionGeneral, L"AutoStart", config.autoStart);
    write(kSectionDisplay, L"FontSize", std::to_wstring(config.fontSize));

    writeBool(kSectionDisplay, L"ShowCpu", config.showCpu);
    writeBool(kSectionDisplay, L"ShowMemory", config.showMemory);
    writeBool(kSectionDisplay, L"MemoryShowPercent", config.memoryShowPercent);
    writeBool(kSectionDisplay, L"GpuMemoryShowPercent", config.gpuMemoryShowPercent);
    writeBool(kSectionDisplay, L"ShowGpu", config.showGpu);
    writeBool(kSectionDisplay, L"ShowNetwork", config.showNetwork);
    writeBool(kSectionDisplay, L"ShowPercentDecimal", config.showPercentDecimal);
    writeBool(kSectionDisplay, L"ShowNetworkArrows", config.showNetworkArrows);
    writeBool(kSectionDisplay, L"IncludeVirtualNetworkInterfaces", config.includeVirtualNetworkInterfaces);
    write(kSectionDisplay, L"SelectedNetworkLuid", std::to_wstring(config.selectedNetworkLuid));
    write(kSectionDisplay, L"SelectedGpuLuid", std::to_wstring(config.selectedGpuLuid));
    writeBool(kSectionDisplay, L"DarkTheme", config.darkTheme);

    writeBool(kSectionHud, L"Locked", config.hudLocked);
    writeBool(kSectionHud, L"ClickThrough", config.hudClickThrough);
    writeBool(kSectionHud, L"NetworkOnly", config.hudNetworkOnly);
    write(kSectionHud, L"LayoutVersion", L"2");
    write(kSectionHud, L"Opacity", std::to_wstring(config.hudOpacity));
    write(kSectionHud, L"WidthDip", std::to_wstring(config.hudWidthDip));
    write(kSectionHud, L"HeightDip", std::to_wstring(config.hudHeightDip));
    write(kSectionHud, L"BorderColor", std::to_wstring(static_cast<unsigned long>(config.hudBorderColor)));
    write(kSectionHud, L"TextColor", std::to_wstring(static_cast<unsigned long>(config.hudTextColor)));
    write(kSectionHud, L"BackgroundColor", std::to_wstring(static_cast<unsigned long>(config.hudBackgroundColor)));
    write(kSectionHud, L"BorderThicknessTenths", std::to_wstring(config.hudBorderThicknessTenths));
    write(kSectionHud, L"ColorPreset", std::to_wstring(config.hudColorPreset));
    write(kSectionHud, L"Left", std::to_wstring(config.hudRect.left));
    write(kSectionHud, L"Top", std::to_wstring(config.hudRect.top));
    write(kSectionHud, L"Right", std::to_wstring(config.hudRect.right));
    write(kSectionHud, L"Bottom", std::to_wstring(config.hudRect.bottom));
    const bool flushed = WritePrivateProfileStringW(nullptr, nullptr, nullptr, path.c_str()) != FALSE;
    return saved && flushed;
}

bool ConfigService::SaveLastGoodHud(const AppConfig& config) const {
    std::error_code error;
    std::filesystem::create_directories(ConfigDirectory(), error);
    if (error) return false;
    const auto path = Path();
    const auto write = [&](const wchar_t* key, int value) {
        WritePrivateProfileStringW(kSectionHudLastGood, key, std::to_wstring(value).c_str(),
                                   path.c_str());
    };
    write(L"Valid", 1); write(L"Left", config.hudRect.left); write(L"Top", config.hudRect.top);
    write(L"WidthDip", config.hudWidthDip); write(L"HeightDip", config.hudHeightDip);
    write(L"FontSize", config.fontSize); write(L"Opacity", config.hudOpacity);
    write(L"BorderThicknessTenths", config.hudBorderThicknessTenths);
    write(L"BorderColor", static_cast<int>(config.hudBorderColor));
    write(L"TextColor", static_cast<int>(config.hudTextColor));
    write(L"BackgroundColor", static_cast<int>(config.hudBackgroundColor));
    write(L"ColorPreset", config.hudColorPreset);
    return true;
}

bool ConfigService::LoadLastGoodHud(AppConfig& config) const {
    const auto path = Path();
    if (ReadInt(path, kSectionHudLastGood, L"Valid", 0) == 0) return false;
    config.hudRect.left = ReadInt(path, kSectionHudLastGood, L"Left", config.hudRect.left);
    config.hudRect.top = ReadInt(path, kSectionHudLastGood, L"Top", config.hudRect.top);
    config.hudWidthDip = ReadInt(path, kSectionHudLastGood, L"WidthDip", config.hudWidthDip);
    config.hudHeightDip = ReadInt(path, kSectionHudLastGood, L"HeightDip", config.hudHeightDip);
    config.fontSize = ReadInt(path, kSectionHudLastGood, L"FontSize", config.fontSize);
    config.hudOpacity = ReadInt(path, kSectionHudLastGood, L"Opacity", config.hudOpacity);
    config.hudBorderThicknessTenths = ReadInt(path, kSectionHudLastGood, L"BorderThicknessTenths", config.hudBorderThicknessTenths);
    config.hudBorderColor = static_cast<COLORREF>(ReadInt(path, kSectionHudLastGood, L"BorderColor", config.hudBorderColor));
    config.hudTextColor = static_cast<COLORREF>(ReadInt(path, kSectionHudLastGood, L"TextColor", config.hudTextColor));
    config.hudBackgroundColor = static_cast<COLORREF>(ReadInt(path, kSectionHudLastGood, L"BackgroundColor", config.hudBackgroundColor));
    config.hudColorPreset = ReadInt(path, kSectionHudLastGood, L"ColorPreset", config.hudColorPreset);
    Normalize(config);
    return true;
}

bool SetAutoStart(bool enabled) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0,
                      KEY_SET_VALUE, &key) != ERROR_SUCCESS) {
        return false;
    }

    if (enabled) {
        wchar_t executable[MAX_PATH]{};
        const DWORD length = GetModuleFileNameW(nullptr, executable, MAX_PATH);
        if (length == 0 || length >= MAX_PATH) {
            RegCloseKey(key);
            return false;
        }
        const std::wstring command = std::wstring(L"\"") + executable + L"\"";
        const bool success = RegSetValueExW(key, L"SysGlance", 0, REG_SZ,
                                            reinterpret_cast<const BYTE*>(command.c_str()),
                                            static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
        RegCloseKey(key);
        return success;
    } else {
        const LSTATUS status = RegDeleteValueW(key, L"SysGlance");
        RegCloseKey(key);
        return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
    }
}

}  // namespace sysglance
