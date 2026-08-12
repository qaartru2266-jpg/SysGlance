#include "config.h"
#include "ui.h"

#include <windows.h>

using namespace sysglance;

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    HANDLE instanceMutex = CreateMutexW(nullptr, TRUE, L"Local\\SysGlance.SingleInstance");
    if (instanceMutex == nullptr || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (instanceMutex != nullptr) CloseHandle(instanceMutex);
        return 0;
    }

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    ConfigService configService;
    AppConfig config = configService.Load();
    AppUi app(instance, config, configService);
    const int result = app.Initialize() ? app.Run() : 1;

    CloseHandle(instanceMutex);
    return result;
}
