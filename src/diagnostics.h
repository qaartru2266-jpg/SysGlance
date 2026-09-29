#pragma once

#include <windows.h>

#include <cstdint>
#include <string>

namespace sysglance {

enum class ExitReason : int {
    Unknown = 0,
    Normal = 1,
    Unclean = 2,
    UnhandledException = 3,
    SystemSessionEnd = 4,
    InitializationFailed = 5,
};

struct RuntimeDiagnostic {
    bool previousSessionWasRunning = false;
    bool recoveredLaunch = false;
    ExitReason lastExitReason = ExitReason::Unknown;
    std::uint64_t lastExitTimestampMs = 0;
    DWORD exceptionCode = 0;
};

// Lifecycle-only diagnostics. It intentionally never receives high-frequency
// metric data, so the log remains useful and small during long-running use.
class DiagnosticService {
public:
    RuntimeDiagnostic BeginSession(bool recoveredLaunch);
    void RecordExit(ExitReason reason, DWORD exceptionCode = 0);
    void RecordUnhandledException(DWORD exceptionCode);
    void Log(const wchar_t* event) const;

    std::wstring DirectoryPath() const;
    std::wstring LogPath() const;

private:
    RuntimeDiagnostic ReadState() const;
    void WriteState(bool running, ExitReason reason, DWORD exceptionCode,
                    std::uint64_t timestampMs) const;
};

bool ConfigureApplicationRecovery(bool enabled);
void InstallDiagnosticExceptionHandler(DiagnosticService* diagnostics);

}  // namespace sysglance
