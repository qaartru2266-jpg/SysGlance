#include "config.h"
#include "diagnostics.h"
#include "ui.h"

#include <windows.h>

using namespace sysglance;

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int) {
    HANDLE instanceMutex = CreateMutexW(nullptr, TRUE, L"Local\\SysGlance.SingleInstance");
    if (instanceMutex == nullptr || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (instanceMutex != nullptr) CloseHandle(instanceMutex);
        return 0;
    }

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    ConfigService configService;
    AppConfig config = configService.Load();
    const bool recoveredLaunch = commandLine != nullptr &&
                               wcsstr(commandLine, L"--sysglance-recovered") != nullptr;
    DiagnosticService diagnostics;
    const RuntimeDiagnostic previousRuntime = diagnostics.BeginSession(recoveredLaunch);
    InstallDiagnosticExceptionHandler(&diagnostics);

    AppUi app(instance, config, configService, diagnostics, previousRuntime);
    const bool initialized = app.Initialize();
    // Do not allow a broken initialization path to create a restart loop. Once
    // the UI and sampling service are ready, Windows can safely relaunch the
    // process after a later unexpected termination.
    ConfigureApplicationRecovery(initialized && config.autoRecover);
    const int result = initialized ? app.Run() : 1;
    if (result == 0) {
        diagnostics.RecordExit(app.ExitReasonOnClose());
    } else if (!initialized) {
        diagnostics.RecordExit(ExitReason::InitializationFailed);
    }

    CloseHandle(instanceMutex);
    return result;
}
