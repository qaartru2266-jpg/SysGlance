#pragma once

#include "common.h"
#include "config.h"
#include "metrics.h"

#include <d2d1.h>
#include <dwrite.h>
#include <shellapi.h>
#include <wrl/client.h>

#include <optional>

namespace sysglance {

class AppUi {
public:
    AppUi(HINSTANCE instance, AppConfig config, ConfigService configService);
    ~AppUi();

    AppUi(const AppUi&) = delete;
    AppUi& operator=(const AppUi&) = delete;

    bool Initialize();
    int Run();

private:
    static LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT message, WPARAM wParam,
                                           LPARAM lParam);
    static LRESULT CALLBACK SurfaceWindowProc(HWND hwnd, UINT message, WPARAM wParam,
                                              LPARAM lParam);
    static LRESULT CALLBACK SettingsWindowProc(HWND hwnd, UINT message, WPARAM wParam,
                                               LPARAM lParam);

    LRESULT HandleMainMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleSurfaceMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleSettingsMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    bool RegisterWindowClasses();
    bool CreateMainWindow();
    HICON CreateTrayIconGraphic() const;
    void CreateTrayIcon();
    void RemoveTrayIcon();
    void CreateSurfaces();
    void DestroySurfaces();
    void ApplyMode();
    void ApplyHudStyle();
    void PositionTaskbarSurface();
    struct HudLayout {
        RECT frame{};
        RECT content{};
        int borderPixels = 1;
        bool canRenderText = false;
    };
    HudLayout CalculateHudLayout(const AppConfig& config, bool clampToMonitor) const;
    void PositionHudSurface();
    bool CanRenderHud(const AppConfig& config) const;
    void SaveHudPlacement();
    void UpdateHudFrameRegion(int width, int height, int borderThickness);
    void RenderHudFrame(HWND hwnd);
    void ChooseHudColor(HWND owner, COLORREF& color);
    void SetHudColorPreset(AppConfig& config, int preset);
    void MoveHudTo(int left, int top);
    void ShowSettings();
    void ApplySettingsFromControls();
    void ReadSettingsControls(AppConfig& config) const;
    void ResetSettingsDraft(bool recommended);
    void PopulateDeviceSelectors();
    void BuildSettingsControls(HWND hwnd);
    void CreateTextFormat();
    void UpdateTrayTooltip();
    void ShowTrayMenu(POINT point);
    void SetDisplayMode(DisplayMode mode);
    void RenderSurface(HWND hwnd, bool hud);
    void DrawSettingsPreview(const DRAWITEMSTRUCT& draw) const;
    void InvalidateSettingsPreview() const;
    void EnsureRenderTarget(HWND hwnd);
    std::wstring MetricsText(const AppConfig& config) const;
    std::wstring TooltipText() const;
    std::wstring FormatRate(double bytesPerSecond) const;
    std::wstring FormatBytes(std::uint64_t bytes) const;
    std::wstring FormatPercent(double percent, const AppConfig& config) const;
    std::wstring FormatFixedNumber(double value, int width, double maximum) const;
    void UpdateSettingsButtonState();

    HINSTANCE instance_ = nullptr;
    ConfigService configService_;
    AppConfig config_;
    std::optional<AppConfig> settingsDraft_;
    MetricService metrics_;
    std::shared_ptr<const MetricSnapshot> latest_;

    HWND mainWindow_ = nullptr;
    HWND taskbarWindow_ = nullptr;
    HWND hudWindow_ = nullptr;
    HWND hudFrameWindow_ = nullptr;
    HWND settingsWindow_ = nullptr;
    HWND settingsPreview_ = nullptr;
    NOTIFYICONDATAW trayIcon_{};
    HICON trayIconHandle_ = nullptr;
    bool trayCreated_ = false;
    bool exiting_ = false;

    HWND modeCombo_ = nullptr;
    HWND intervalCombo_ = nullptr;
    HWND cpuCheck_ = nullptr;
    HWND memoryCheck_ = nullptr;
    HWND memoryModeCombo_ = nullptr;
    HWND gpuMemoryModeCombo_ = nullptr;
    HWND percentPrecisionCombo_ = nullptr;
    HWND networkArrowsCheck_ = nullptr;
    HWND gpuCheck_ = nullptr;
    HWND networkCheck_ = nullptr;
    HWND darkCheck_ = nullptr;
    HWND lockedCheck_ = nullptr;
    HWND clickThroughCheck_ = nullptr;
    HWND autoStartCheck_ = nullptr;
    HWND opacityEdit_ = nullptr;
    HWND hudWidthEdit_ = nullptr;
    HWND hudHeightEdit_ = nullptr;
    HWND fontSizeEdit_ = nullptr;
    HWND borderThicknessEdit_ = nullptr;
    HWND colorPresetCombo_ = nullptr;
    HWND networkInterfaceCombo_ = nullptr;
    HWND includeVirtualNetworkCheck_ = nullptr;
    HWND gpuAdapterCombo_ = nullptr;
    HWND previewWarning_ = nullptr;

    bool hudDragging_ = false;
    bool hudLayoutDirty_ = true;
    std::uint64_t hudFeedbackUntil_ = 0;
    POINT hudDragStartCursor_{};
    RECT hudDragStartRect_{};

    Microsoft::WRL::ComPtr<ID2D1Factory> d2dFactory_;
    Microsoft::WRL::ComPtr<IDWriteFactory> writeFactory_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> textFormat_;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> taskbarRenderTarget_;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> hudRenderTarget_;
};

}  // namespace sysglance
