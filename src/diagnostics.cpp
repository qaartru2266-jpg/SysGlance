#include "diagnostics.h"

#include <shlobj.h>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace sysglance {
namespace {

constexpr wchar_t kDiagnosticsSection[] = L"Runtime";
constexpr wchar_t kStateFile[] = L"runtime.ini";
constexpr wchar_t kLogFile[] = L"diagnostics.log";
constexpr std::uintmax_t kMaximumLogBytes = 256 * 1024;

DiagnosticService* g_diagnostics = nullptr;

std::wstring LocalAppData() {
    wchar_t path[MAX_PATH]{};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT,
                                   path))) {
        return path;
    }
    return L".";
}

std::uint64_t CurrentTimestampMs() {
    FILETIME time{};
    GetSystemTimeAsFileTime(&time);
    ULARGE_INTEGER value{};
    value.LowPart = time.dwLowDateTime;
    value.HighPart = time.dwHighDateTime;
    return value.QuadPart / 10'000;
}

std::uint64_t ReadUInt64(const std::wstring& path, const wchar_t* key, std::uint64_t fallback) {
    wchar_t value[64]{};
    const auto fallbackText = std::to_wstring(fallback);
    GetPrivateProfileStringW(kDiagnosticsSection, key, fallbackText.c_str(), value,
                             static_cast<DWORD>(std::size(value)), path.c_str());
    wchar_t* end = nullptr;
    const auto parsed = _wcstoui64(value, &end, 10);
    return end == value ? fallback : parsed;
}

LONG WINAPI UnhandledExceptionFilter(EXCEPTION_POINTERS* pointers) {
    if (g_diagnostics != nullptr && pointers != nullptr && pointers->ExceptionRecord != nullptr) {
        g_diagnostics->RecordUnhandledException(pointers->ExceptionRecord->ExceptionCode);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

}  // namespace

std::wstring DiagnosticService::DirectoryPath() const {
    return LocalAppData() + L"\\SysGlance";
}

std::wstring DiagnosticService::LogPath() const {
    return DirectoryPath() + L"\\" + kLogFile;
}

RuntimeDiagnostic DiagnosticService::ReadState() const {
    const std::wstring statePath = DirectoryPath() + L"\\" + kStateFile;
    RuntimeDiagnostic result;
    result.previousSessionWasRunning =
        GetPrivateProfileIntW(kDiagnosticsSection, L"Running", 0, statePath.c_str()) != 0;
    result.lastExitReason = static_cast<ExitReason>(
        GetPrivateProfileIntW(kDiagnosticsSection, L"LastExitReason", 0, statePath.c_str()));
    result.lastExitTimestampMs = ReadUInt64(statePath, L"LastExitTimestampMs", 0);
    result.exceptionCode = static_cast<DWORD>(
        GetPrivateProfileIntW(kDiagnosticsSection, L"LastExceptionCode", 0, statePath.c_str()));
    return result;
}

void DiagnosticService::WriteState(bool running, ExitReason reason, DWORD exceptionCode,
                                   std::uint64_t timestampMs) const {
    std::error_code error;
    std::filesystem::create_directories(DirectoryPath(), error);
    if (error) return;

    const std::wstring statePath = DirectoryPath() + L"\\" + kStateFile;
    // Write the tiny state file directly instead of relying on the legacy INI
    // cache. A process can end immediately after this call, so an explicit
    // close is more useful here than delayed profile-API flushing.
    std::ofstream state(std::filesystem::path(statePath), std::ios::trunc);
    if (!state) return;
    state << "[Runtime]\n"
          << "Running=" << (running ? 1 : 0) << '\n'
          << "LastExitReason=" << static_cast<int>(reason) << '\n'
          << "LastExitTimestampMs=" << timestampMs << '\n'
          << "LastExceptionCode=" << exceptionCode << '\n';
}

RuntimeDiagnostic DiagnosticService::BeginSession(bool recoveredLaunch) {
    RuntimeDiagnostic previous = ReadState();
    previous.recoveredLaunch = recoveredLaunch;
    if (previous.previousSessionWasRunning) {
        previous.lastExitReason = ExitReason::Unclean;
        previous.lastExitTimestampMs = CurrentTimestampMs();
        previous.exceptionCode = 0;
        Log(L"Previous session had no graceful exit marker; recorded as unclean termination.");
    }
    WriteState(true, previous.lastExitReason, previous.exceptionCode, previous.lastExitTimestampMs);
    Log(recoveredLaunch ? L"Session started by Windows automatic recovery."
                        : L"Session started.");
    return previous;
}

void DiagnosticService::RecordExit(ExitReason reason, DWORD exceptionCode) {
    WriteState(false, reason, exceptionCode, CurrentTimestampMs());
    switch (reason) {
        case ExitReason::Normal:
            Log(L"Session ended normally.");
            break;
        case ExitReason::UnhandledException:
            Log(L"Unhandled exception captured before termination.");
            break;
        case ExitReason::SystemSessionEnd:
            Log(L"Session ended because Windows ended the user session.");
            break;
        case ExitReason::InitializationFailed:
            Log(L"Session ended because initialization failed.");
            break;
        default:
            Log(L"Session ended without a graceful completion marker.");
            break;
    }
}

void DiagnosticService::RecordUnhandledException(DWORD exceptionCode) {
    // Best effort only: Windows may terminate the process before any user-mode
    // handler runs, which is why BeginSession also detects an uncleared marker.
    RecordExit(ExitReason::UnhandledException, exceptionCode);
}

void DiagnosticService::Log(const wchar_t* event) const {
    std::error_code error;
    std::filesystem::create_directories(DirectoryPath(), error);
    if (error) return;

    const auto path = LogPath();
    if (std::filesystem::exists(path, error) && !error &&
        std::filesystem::file_size(path, error) > kMaximumLogBytes) {
        std::filesystem::remove(path, error);
    }

    SYSTEMTIME time{};
    GetLocalTime(&time);
    std::ofstream stream(std::filesystem::path(path), std::ios::app);
    if (!stream) return;
    stream << std::setfill('0') << std::setw(4) << time.wYear << '-' << std::setw(2)
           << time.wMonth << '-' << std::setw(2) << time.wDay << ' ' << std::setw(2)
           << time.wHour << ':' << std::setw(2) << time.wMinute << ':' << std::setw(2)
           << time.wSecond << " | ";
    for (const wchar_t* current = event; *current != L'\0'; ++current) {
        stream << static_cast<char>(*current <= 0x7f ? *current : '?');
    }
    stream << '\n';
}

bool ConfigureApplicationRecovery(bool enabled) {
    if (!enabled) {
        return UnregisterApplicationRestart() == S_OK;
    }
    // Let Windows restart only after an unexpected termination. Normal exits,
    // hangs, patches, and shutdown/reboot paths do not trigger a relaunch.
    return RegisterApplicationRestart(L"--sysglance-recovered",
                                      RESTART_NO_HANG | RESTART_NO_PATCH | RESTART_NO_REBOOT) == S_OK;
}

void InstallDiagnosticExceptionHandler(DiagnosticService* diagnostics) {
    g_diagnostics = diagnostics;
    SetUnhandledExceptionFilter(UnhandledExceptionFilter);
}

}  // namespace sysglance
