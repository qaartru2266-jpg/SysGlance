#include "ui.h"
#include "resource.h"

#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>

#include <cmath>
#include <cwchar>
#include <iomanip>
#include <sstream>
#include <utility>

namespace sysglance {
namespace {

constexpr wchar_t kMainClass[] = L"SysGlance.MainWindow";
constexpr wchar_t kSurfaceClass[] = L"SysGlance.SurfaceWindow";
constexpr wchar_t kSettingsClass[] = L"SysGlance.SettingsWindow";
constexpr UINT kMetricsReadyMessage = WM_APP + 1;
constexpr UINT kTrayMessage = WM_APP + 2;
constexpr int kTrayId = 100;
constexpr int kModeComboId = 200;
constexpr int kIntervalComboId = 201;
constexpr int kCpuCheckId = 202;
constexpr int kMemoryCheckId = 203;
constexpr int kMemoryModeComboId = 215;
constexpr int kGpuCheckId = 204;
constexpr int kNetworkCheckId = 205;
constexpr int kDarkCheckId = 206;
constexpr int kLockedCheckId = 207;
constexpr int kClickThroughCheckId = 208;
constexpr int kAutoStartCheckId = 209;
constexpr int kOpacityEditId = 210;
constexpr int kApplyButtonId = 211;
constexpr int kCloseButtonId = 212;
constexpr int kFontSizeEditId = 213;
constexpr int kBorderColorButtonId = 214;
constexpr int kTextColorButtonId = 216;
constexpr int kBackgroundColorButtonId = 217;
constexpr int kBorderThicknessEditId = 218;
constexpr int kColorPresetComboId = 219;
constexpr int kHudWidthEditId = 220;
constexpr int kHudHeightEditId = 221;
constexpr int kSettingsPreviewId = 222;
constexpr int kGpuMemoryModeComboId = 223;
constexpr int kPercentPrecisionComboId = 224;
constexpr int kNetworkArrowsCheckId = 225;
constexpr int kNetworkInterfaceComboId = 226;
constexpr int kIncludeVirtualNetworkCheckId = 227;
constexpr int kGpuAdapterComboId = 228;
constexpr int kRecommendedHudButtonId = 229;
constexpr int kLastGoodHudButtonId = 230;
constexpr int kExitMenuId = 300;
constexpr int kSettingsMenuId = 301;
constexpr int kTrayModeMenuId = 310;
constexpr int kTaskbarModeMenuId = 311;
constexpr int kHudModeMenuId = 312;
constexpr int kTaskbarWidth = 360;
constexpr int kTaskbarHeight = 30;
constexpr int kSettingsWidthDip = 570;
constexpr int kSettingsContentHeightDip = 1010;
constexpr int kSettingsWindowMarginDip = 32;

std::wstring Number(double value, int precision = 0) {
    std::wstringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

void SetControlFont(HWND control) {
    SendMessageW(control, WM_SETFONT,
                 reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
}

int ReadPositiveIntegerInput(HWND control, int fallback, int maximum = 32000) {
    wchar_t text[32]{};
    GetWindowTextW(control, text, _countof(text));
    wchar_t* end = nullptr;
    const double value = std::wcstod(text, &end);
    if (end == text || !std::isfinite(value) || value <= 0.0) {
        return fallback;
    }
    return std::clamp(static_cast<int>(std::lround(value)), 1, maximum);
}

int ReadPositiveTenthsInput(HWND control, int fallback) {
    wchar_t text[32]{};
    GetWindowTextW(control, text, _countof(text));
    wchar_t* end = nullptr;
    const double value = std::wcstod(text, &end);
    if (end == text || !std::isfinite(value) || value <= 0.0) {
        return fallback;
    }
    return std::clamp(static_cast<int>(std::lround(value * 10.0)), 1, 1000);
}

D2D1_COLOR_F ToD2DColor(COLORREF color, float alpha) {
    return D2D1::ColorF(static_cast<float>(GetRValue(color)) / 255.0f,
                         static_cast<float>(GetGValue(color)) / 255.0f,
                         static_cast<float>(GetBValue(color)) / 255.0f, alpha);
}

void PresetColors(int preset, COLORREF& background, COLORREF& text, COLORREF& border) {
    switch (preset) {
        case 0:
            background = RGB(10, 13, 18);
            text = RGB(255, 255, 255);
            border = RGB(255, 96, 0);
            break;
        case 1:
            background = RGB(235, 244, 255);
            text = RGB(16, 42, 78);
            border = RGB(0, 140, 255);
            break;
        case 2:
            background = RGB(7, 26, 22);
            text = RGB(222, 255, 244);
            border = RGB(0, 245, 170);
            break;
        default:
            break;
    }
}

int PixelsFromDip(float value, UINT dpi) {
    return static_cast<int>(std::lround(value * static_cast<float>(dpi) / 96.0f));
}

}  // namespace

AppUi::AppUi(HINSTANCE instance, AppConfig config, ConfigService configService)
    : instance_(instance), configService_(std::move(configService)), config_(config) {}

AppUi::~AppUi() {
    exiting_ = true;
    metrics_.Stop();
    RemoveTrayIcon();
    DestroySurfaces();
    if (settingsWindow_ != nullptr) {
        DestroyWindow(settingsWindow_);
    }
    if (settingsFont_ != nullptr) {
        DeleteObject(settingsFont_);
        settingsFont_ = nullptr;
    }
    if (mainWindow_ != nullptr) {
        DestroyWindow(mainWindow_);
    }
}

bool AppUi::RegisterWindowClasses() {
    WNDCLASSEXW mainClass{sizeof(WNDCLASSEXW)};
    mainClass.hInstance = instance_;
    mainClass.lpfnWndProc = MainWindowProc;
    mainClass.lpszClassName = kMainClass;
    mainClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassExW(&mainClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    WNDCLASSEXW surfaceClass{sizeof(WNDCLASSEXW)};
    surfaceClass.hInstance = instance_;
    surfaceClass.style = CS_DBLCLKS;
    surfaceClass.lpfnWndProc = SurfaceWindowProc;
    surfaceClass.lpszClassName = kSurfaceClass;
    surfaceClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    surfaceClass.hbrBackground = nullptr;
    if (!RegisterClassExW(&surfaceClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    WNDCLASSEXW settingsClass{sizeof(WNDCLASSEXW)};
    settingsClass.hInstance = instance_;
    settingsClass.lpfnWndProc = SettingsWindowProc;
    settingsClass.lpszClassName = kSettingsClass;
    settingsClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    settingsClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if (!RegisterClassExW(&settingsClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    return true;
}

bool AppUi::Initialize() {
    if (!RegisterWindowClasses() || !CreateMainWindow()) {
        return false;
    }

    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory),
                      reinterpret_cast<void**>(d2dFactory_.GetAddressOf()));
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                        reinterpret_cast<IUnknown**>(writeFactory_.GetAddressOf()));
    CreateTextFormat();

    CreateTrayIcon();
    CreateSurfaces();
    latest_ = metrics_.Snapshot();
    UpdateTrayTooltip();
    ApplyMode();
    metrics_.SetNetworkSelection(config_.selectedNetworkLuid,
                                 config_.includeVirtualNetworkInterfaces);
    metrics_.SetGpuSelection(config_.selectedGpuLuid);
    metrics_.Start(config_.refreshIntervalMs, mainWindow_);
    return true;
}

void AppUi::CreateTextFormat() {
    textFormat_.Reset();
    if (!writeFactory_) {
        return;
    }
    writeFactory_->CreateTextFormat(
        // Consolas uses tabular glyphs. Together with the fixed-width fields below, this
        // keeps every metric in the same place while the sampled values change.
        L"Consolas", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, static_cast<float>(config_.fontSize), L"zh-CN",
        textFormat_.GetAddressOf());
    if (textFormat_) {
        textFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        textFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        textFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    }
}

int AppUi::Run() {
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

bool AppUi::CreateMainWindow() {
    mainWindow_ = CreateWindowExW(0, kMainClass, L"SysGlance", 0, 0, 0, 0, 0,
                                  HWND_MESSAGE, nullptr, instance_, this);
    if (mainWindow_ == nullptr) {
        return false;
    }
    return true;
}

HICON AppUi::CreateTrayIconGraphic() const {
    return static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(IDI_SYSGLANCE), IMAGE_ICON,
                                         32, 32, LR_DEFAULTCOLOR));
}

void AppUi::CreateTrayIcon() {
    trayIcon_.cbSize = sizeof(trayIcon_);
    trayIcon_.hWnd = mainWindow_;
    trayIcon_.uID = kTrayId;
    trayIcon_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    trayIcon_.uCallbackMessage = kTrayMessage;
    trayIconHandle_ = CreateTrayIconGraphic();
    trayIcon_.hIcon = trayIconHandle_ != nullptr ? trayIconHandle_ : LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(trayIcon_.szTip, _countof(trayIcon_.szTip), L"SysGlance");
    trayCreated_ = Shell_NotifyIconW(NIM_ADD, &trayIcon_) == TRUE;
}

void AppUi::RemoveTrayIcon() {
    if (trayCreated_) {
        Shell_NotifyIconW(NIM_DELETE, &trayIcon_);
        trayCreated_ = false;
    }
    if (trayIconHandle_ != nullptr) {
        DestroyIcon(trayIconHandle_);
        trayIconHandle_ = nullptr;
    }
}

void AppUi::CreateSurfaces() {
    taskbarWindow_ = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST, kSurfaceClass, L"SysGlance",
        WS_POPUP, 0, 0, kTaskbarWidth, kTaskbarHeight, nullptr, nullptr, instance_, this);
    if (taskbarWindow_ != nullptr && trayIconHandle_ != nullptr) {
        SendMessageW(taskbarWindow_, WM_SETICON, ICON_SMALL,
                     reinterpret_cast<LPARAM>(trayIconHandle_));
        SendMessageW(taskbarWindow_, WM_SETICON, ICON_BIG,
                     reinterpret_cast<LPARAM>(trayIconHandle_));
    }
    hudFrameWindow_ = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST, kSurfaceClass, L"SysGlance HUD Frame",
        WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, instance_, this);
    hudWindow_ = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_LAYERED, kSurfaceClass,
        L"SysGlance HUD", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, instance_,
        this);
    ApplyHudStyle();
}

void AppUi::DestroySurfaces() {
    if (taskbarWindow_ != nullptr) {
        DestroyWindow(taskbarWindow_);
        taskbarWindow_ = nullptr;
    }
    if (hudWindow_ != nullptr) {
        DestroyWindow(hudWindow_);
        hudWindow_ = nullptr;
    }
    if (hudFrameWindow_ != nullptr) {
        DestroyWindow(hudFrameWindow_);
        hudFrameWindow_ = nullptr;
    }
    taskbarRenderTarget_.Reset();
    hudRenderTarget_.Reset();
}

void AppUi::ApplyMode() {
    if (taskbarWindow_ == nullptr || hudWindow_ == nullptr || hudFrameWindow_ == nullptr) {
        return;
    }
    const bool taskbarMode = config_.displayMode == DisplayMode::Taskbar;
    ShowWindow(taskbarWindow_, taskbarMode ? SW_SHOWNOACTIVATE : SW_HIDE);
    if (taskbarMode) PositionTaskbarSurface();
    const int hudVisibility = config_.displayMode == DisplayMode::Hud ? SW_SHOWNOACTIVATE : SW_HIDE;
    ShowWindow(hudWindow_, hudVisibility);
    ShowWindow(hudFrameWindow_, hudVisibility);
    if (config_.displayMode == DisplayMode::Hud) {
        PositionHudSurface();
    }
    InvalidateRect(taskbarWindow_, nullptr, FALSE);
    InvalidateRect(hudWindow_, nullptr, FALSE);
    InvalidateRect(hudFrameWindow_, nullptr, FALSE);
}

void AppUi::ApplyHudStyle() {
    if (hudWindow_ == nullptr) {
        return;
    }
    SetLayeredWindowAttributes(hudWindow_, 0, static_cast<BYTE>(config_.hudOpacity), LWA_ALPHA);
    PositionHudSurface();
    InvalidateRect(hudWindow_, nullptr, FALSE);
    InvalidateRect(hudFrameWindow_, nullptr, FALSE);
}

void AppUi::PositionTaskbarSurface() {
    if (taskbarWindow_ == nullptr) {
        return;
    }

    APPBARDATA taskbarInfo{sizeof(taskbarInfo)};
    taskbarInfo.uEdge = ABE_BOTTOM;
    RECT taskbar{0, 0, 0, 0};
    if (SHAppBarMessage(ABM_GETTASKBARPOS, &taskbarInfo) != 0) {
        taskbar = taskbarInfo.rc;
    } else {
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &taskbar, 0);
        taskbar.top = taskbar.bottom;
    }

    int x = taskbar.right - kTaskbarWidth - 16;
    int y = taskbar.top - kTaskbarHeight;
    switch (taskbarInfo.uEdge) {
        case ABE_TOP:
            x = taskbar.right - kTaskbarWidth - 16;
            y = taskbar.bottom;
            break;
        case ABE_LEFT:
            x = taskbar.right;
            y = taskbar.bottom - kTaskbarHeight - 16;
            break;
        case ABE_RIGHT:
            x = taskbar.left - kTaskbarWidth;
            y = taskbar.bottom - kTaskbarHeight - 16;
            break;
        default:
            break;
    }
    SetWindowPos(taskbarWindow_, HWND_TOPMOST, x, y, kTaskbarWidth, kTaskbarHeight,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

AppUi::HudLayout AppUi::CalculateHudLayout(const AppConfig& config, bool clampToMonitor) const {
    HudLayout layout;
    const UINT dpi = hudWindow_ != nullptr ? GetDpiForWindow(hudWindow_) : USER_DEFAULT_SCREEN_DPI;
    const int logicalWidth = config.hudNetworkOnly ? std::max(1, config.hudWidthDip / 2)
                                                    : config.hudWidthDip;
    const int contentWidth = std::max(1, PixelsFromDip(static_cast<float>(logicalWidth), dpi));
    const int contentHeight = std::max(1, PixelsFromDip(static_cast<float>(config.hudHeightDip), dpi));
    layout.borderPixels = std::max(1, PixelsFromDip(
        static_cast<float>(config.hudBorderThicknessTenths) / 10.0f, dpi));
    const int outerWidth = contentWidth + layout.borderPixels * 2;
    const int outerHeight = contentHeight + layout.borderPixels * 2;
    RECT frame = config.hudRect;
    if (frame.right <= frame.left || frame.bottom <= frame.top) {
        HMONITOR monitor = MonitorFromWindow(hudWindow_, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO info{sizeof(info)};
        GetMonitorInfoW(monitor, &info);
        frame.left = info.rcWork.right - outerWidth - PixelsFromDip(20.0f, dpi);
        frame.top = info.rcWork.bottom - outerHeight - PixelsFromDip(60.0f, dpi);
    }
    if (clampToMonitor) {
        HMONITOR monitor = MonitorFromRect(&frame, MONITOR_DEFAULTTONULL);
        if (monitor == nullptr) monitor = MonitorFromWindow(hudWindow_, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO info{sizeof(info)};
        GetMonitorInfoW(monitor, &info);
        frame.left = std::clamp(frame.left, info.rcWork.left,
                                std::max(info.rcWork.left, info.rcWork.right - outerWidth));
        frame.top = std::clamp(frame.top, info.rcWork.top,
                               std::max(info.rcWork.top, info.rcWork.bottom - outerHeight));
    }
    frame.right = frame.left + outerWidth;
    frame.bottom = frame.top + outerHeight;
    layout.frame = frame;
    layout.content = {frame.left + layout.borderPixels, frame.top + layout.borderPixels,
                      frame.right - layout.borderPixels, frame.bottom - layout.borderPixels};
    layout.canRenderText = contentWidth >= std::max(20, PixelsFromDip(
                               static_cast<float>(config.fontSize) * 3.0f, dpi)) &&
                           contentHeight >= std::max(10, PixelsFromDip(
                               static_cast<float>(config.fontSize) * 1.15f, dpi));
    return layout;
}

bool AppUi::CanRenderHud(const AppConfig& config) const {
    return CalculateHudLayout(config, false).canRenderText;
}

void AppUi::PositionHudSurface() {
    if (hudWindow_ == nullptr || hudFrameWindow_ == nullptr) {
        return;
    }
    const auto layout = CalculateHudLayout(config_, true);
    config_.hudRect = layout.frame;
    RECT current{};
    GetWindowRect(hudFrameWindow_, &current);
    if (current.left == layout.frame.left && current.top == layout.frame.top &&
        current.right == layout.frame.right && current.bottom == layout.frame.bottom) {
        return;
    }
    SetWindowPos(hudWindow_, HWND_TOPMOST, layout.content.left, layout.content.top,
                 layout.content.right - layout.content.left, layout.content.bottom - layout.content.top,
                 SWP_NOACTIVATE);
    SetWindowPos(hudFrameWindow_, HWND_TOPMOST, layout.frame.left, layout.frame.top,
                 layout.frame.right - layout.frame.left, layout.frame.bottom - layout.frame.top,
                 SWP_NOACTIVATE);
    UpdateHudFrameRegion(layout.frame.right - layout.frame.left, layout.frame.bottom - layout.frame.top,
                         layout.borderPixels);
}

void AppUi::SaveHudPlacement() {
    if (hudFrameWindow_ == nullptr) {
        return;
    }
    GetWindowRect(hudFrameWindow_, &config_.hudRect);
    configService_.Save(config_);
}

void AppUi::UpdateHudFrameRegion(int width, int height, int borderThickness) {
    if (hudFrameWindow_ == nullptr) {
        return;
    }
    HRGN frame = CreateRectRgn(0, 0, width, height);
    const bool hasInterior = width > borderThickness * 2 && height > borderThickness * 2;
    HRGN interior = hasInterior ? CreateRectRgn(borderThickness, borderThickness,
                                                 width - borderThickness, height - borderThickness)
                                : nullptr;
    if (frame == nullptr || (hasInterior && interior == nullptr)) {
        if (frame != nullptr) DeleteObject(frame);
        if (interior != nullptr) DeleteObject(interior);
        return;
    }
    if (interior != nullptr) {
        CombineRgn(frame, frame, interior, RGN_DIFF);
        DeleteObject(interior);
    }
    if (SetWindowRgn(hudFrameWindow_, frame, FALSE) == 0) {
        DeleteObject(frame);
    }
}

void AppUi::RenderHudFrame(HWND hwnd) {
    PAINTSTRUCT paint{};
    HDC dc = BeginPaint(hwnd, &paint);
    HBRUSH brush = CreateSolidBrush(config_.hudBorderColor);
    FillRect(dc, &paint.rcPaint, brush);
    DeleteObject(brush);
    EndPaint(hwnd, &paint);
}

void AppUi::ChooseHudColor(HWND owner, COLORREF& color) {
    static COLORREF customColors[16]{};
    CHOOSECOLORW dialog{sizeof(dialog)};
    dialog.hwndOwner = owner;
    dialog.lpCustColors = customColors;
    dialog.rgbResult = color;
    dialog.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (!ChooseColorW(&dialog)) {
        return;
    }
    color = dialog.rgbResult;
}

void AppUi::SetHudColorPreset(AppConfig& config, int preset) {
    if (preset < 0 || preset > 2) return;
    PresetColors(preset, config.hudBackgroundColor, config.hudTextColor,
                 config.hudBorderColor);
    config.hudColorPreset = preset;
}

void AppUi::InvalidateSettingsPreview() const {
    if (settingsPreview_ != nullptr) {
        InvalidateRect(settingsPreview_, nullptr, FALSE);
    }
}

void AppUi::DrawSettingsPreview(const DRAWITEMSTRUCT& draw) const {
    AppConfig preview = settingsDraft_ ? *settingsDraft_ : config_;
    ReadSettingsControls(preview);
    COLORREF background = preview.hudBackgroundColor;
    COLORREF text = preview.hudTextColor;
    COLORREF border = preview.hudBorderColor;
    int thicknessTenths = preview.hudBorderThicknessTenths;

    if (colorPresetCombo_ != nullptr) {
        const int preset = static_cast<int>(SendMessageW(colorPresetCombo_, CB_GETCURSEL, 0, 0));
        if (preset >= 0) {
            PresetColors(preset, background, text, border);
        }
    }
    if (borderThicknessEdit_ != nullptr) {
        thicknessTenths = ReadPositiveTenthsInput(borderThicknessEdit_, thicknessTenths);
    }

    HBRUSH backgroundBrush = CreateSolidBrush(background);
    FillRect(draw.hDC, &draw.rcItem, backgroundBrush);
    DeleteObject(backgroundBrush);

    HBRUSH borderBrush = CreateSolidBrush(border);
    RECT frame = draw.rcItem;
    const int thickness = std::min(std::max(1, (thicknessTenths + 5) / 10), 8);
    for (int layer = 0; layer < thickness && frame.right > frame.left && frame.bottom > frame.top;
         ++layer) {
        FrameRect(draw.hDC, &frame, borderBrush);
        InflateRect(&frame, -1, -1);
    }
    DeleteObject(borderBrush);

    const int scaledFont = std::clamp(static_cast<int>(std::lround(preview.fontSize * 1.15)), 6, 48);
    HFONT font = CreateFontW(-scaledFont, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             FIXED_PITCH | FF_MODERN, L"Consolas");
    HGDIOBJ oldFont = SelectObject(draw.hDC, font);
    SetBkMode(draw.hDC, TRANSPARENT);
    SetTextColor(draw.hDC, text);
    const auto sample = MetricsText(preview);
    DrawTextW(draw.hDC, sample.c_str(), static_cast<int>(sample.size()), &frame,
              DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(draw.hDC, oldFont);
    DeleteObject(font);
}

void AppUi::MoveHudTo(int left, int top) {
    config_.hudRect.left = left;
    config_.hudRect.top = top;
    config_.hudRect.right = left + 1;
    config_.hudRect.bottom = top + 1;
    PositionHudSurface();
}

void AppUi::UpdateTrayTooltip() {
    if (!trayCreated_) {
        return;
    }
    const auto text = TooltipText();
    wcsncpy_s(trayIcon_.szTip, _countof(trayIcon_.szTip), text.c_str(), _TRUNCATE);
    trayIcon_.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &trayIcon_);
}

std::wstring AppUi::FormatBytes(std::uint64_t bytes) const {
    constexpr double kGiB = 1024.0 * 1024.0 * 1024.0;
    // The compact actual-memory display deliberately omits the GiB suffix.
    return FormatFixedNumber(static_cast<double>(bytes) / kGiB, 4, 99.95);
}

std::wstring AppUi::FormatCpuPercent(double percent, const AppConfig& config) const {
    // CPU is the first HUD field, so pad the number itself (after C), never before C
    // or after it. This keeps C09.0/C11.0 and C09/C11 equally wide.
    percent = std::clamp(percent, 0.0, 99.9);
    std::wstring result;
    if (config.showPercentDecimal) {
        result = Number(percent, 1);
        if (result.size() == 3) result.insert(0, 1, L'0');
    } else {
        result = std::to_wstring(std::min(99, static_cast<int>(std::lround(percent))));
        if (result.size() == 1) result.insert(0, 1, L'0');
    }
    return result;
}

std::wstring AppUi::FormatMemoryPercent(double percent, const AppConfig& config) const {
    percent = std::clamp(percent, 0.0, 99.9);
    if (config.showPercentDecimal) {
        std::wstring result = Number(percent, 1);
        if (result.size() == 3) result.insert(0, 1, L'0');
        return result;
    }
    std::wstring result = std::to_wstring(std::min(99, static_cast<int>(std::lround(percent))));
    if (result.size() == 1) result.insert(0, 1, L'0');
    return result;
}

std::wstring AppUi::FormatMemoryBytes(std::uint64_t bytes) const {
    constexpr double kGiB = 1024.0 * 1024.0 * 1024.0;
    std::wstring result = Number(std::clamp(static_cast<double>(bytes) / kGiB, 0.0, 99.9), 1);
    if (result.size() == 3) result.insert(0, 1, L'0');
    return result;
}

std::wstring AppUi::FormatPercent(double percent, const AppConfig& config) const {
    percent = std::clamp(percent, 0.0, 99.9);
    if (config.showPercentDecimal) {
        std::wstring result = Number(percent, 1);
        if (result.size() == 3) result.insert(0, 1, L'0');
        return result;
    }
    std::wstring result = std::to_wstring(std::min(99, static_cast<int>(std::lround(percent))));
    if (result.size() == 1) result.insert(0, 1, L'0');
    return result;
}

std::wstring AppUi::FormatFixedNumber(double value, int width, double maximum) const {
    value = std::max(0.0, value);
    std::wstring result;
    if (value >= maximum) {
        result = std::to_wstring(static_cast<int>(maximum)) + L"+";
    } else {
        result = Number(value, 1);
    }
    if (static_cast<int>(result.size()) > width) {
        result = result.substr(0, static_cast<size_t>(width));
    }
    return result;
}

std::wstring AppUi::FormatRate(double bytesPerSecond) const {
    return FormatNetworkRate(bytesPerSecond);
}

std::wstring AppUi::MetricsText(const AppConfig& config) const {
    if (!latest_) {
        return L"SysGlance";
    }
    // Each network rate is a five-character slot regardless of its magnitude
    // or whether the first sample is still establishing a baseline. That keeps
    // the complete HUD text length and its centered column positions stable.
    const bool networkReady = latest_->networkAvailable && latest_->networkReady;
    const auto downRate = networkReady ? FormatRate(latest_->networkDownloadBytesPerSecond)
                                       : FormatUnavailableNetworkRate();
    const auto upRate = networkReady ? FormatRate(latest_->networkUploadBytesPerSecond)
                                     : FormatUnavailableNetworkRate();
    const auto networkText = config.showNetworkArrows
                                 ? L"↓" + downRate + L" ↑" + upRate
                                 : downRate + L" " + upRate;
    if (config.hudNetworkOnly) {
        return config.showNetwork ? networkText : L"网络已隐藏";
    }
    std::wstring text;
    const auto appendToken = [&text](std::wstring token, size_t slot = 0, bool separator = true) {
        if (!text.empty() && separator) {
            text += L" ";
        }
        text += std::move(token);
        if (slot != 0) {
            const size_t start = text.find_last_of(L' ');
            const size_t used = text.size() - (start == std::wstring::npos ? 0 : start + 1);
            if (used < slot) text.append(slot - used, L' ');
        }
    };
    if (config.showCpu) {
        appendToken(L"C" + FormatCpuPercent(latest_->cpuPercent, config));
    }
    if (config.showMemory) {
        if (latest_->memoryTotalBytes == 0) {
            appendToken(L"N/A", 4);
        } else if (config.memoryShowPercent) {
            appendToken(FormatMemoryPercent(latest_->memoryPercent, config));
        } else {
            appendToken(FormatMemoryBytes(latest_->memoryUsedBytes));
        }
    }
    if (config.showGpu) {
        // Reserve only the width required by the active GPU representation.
        // Integer GPU percentage + integer GPU-memory percentage is G04/06
        // (six characters), so a universal nine-character reserve would leave
        // an unnecessary visible gap before the network fields.
        const size_t gpuPercentWidth = config.showPercentDecimal ? 4 : 2;
        const size_t gpuMemoryWidth = config.gpuMemoryShowPercent ? gpuPercentWidth : 4;
        const size_t gpuSlotWidth = 2 + gpuPercentWidth + gpuMemoryWidth;  // G + / + values
        if (latest_->gpuAvailable) {
            const auto memory = latest_->gpuMemoryAvailable
                                    ? config.gpuMemoryShowPercent
                                          ? latest_->gpuMemoryTotalBytes != 0
                                                ? FormatPercent(
                                                      static_cast<double>(latest_->gpuMemoryUsedBytes) *
                                                      100.0 /
                                                      static_cast<double>(latest_->gpuMemoryTotalBytes), config)
                                                : L"  N/A"
                                          : FormatBytes(latest_->gpuMemoryUsedBytes)
                                    : L"  N/A";
            appendToken(L"G" + FormatPercent(latest_->gpuUtilPercent, config) + L"/" +
                        memory, gpuSlotWidth);
        } else {
            appendToken(L"GN/A/N/A", gpuSlotWidth);
        }
    }
    if (config.showNetwork) {
        appendToken(networkText);
    }
    while (!text.empty() && text.back() == L' ') text.pop_back();
    return text.empty() ? L"SysGlance" : text;
}

std::wstring AppUi::TooltipText() const {
    if (!latest_) return L"SysGlance";
    std::wstringstream text;
    text << L"CPU " << Number(latest_->cpuPercent, 1) << L"% | Memory "
         << Number(static_cast<double>(latest_->memoryUsedBytes) / (1024.0 * 1024.0 * 1024.0), 1)
         << L" GiB (" << Number(latest_->memoryPercent, 1) << L"%)";
    if (latest_->gpuAvailable) {
        text << L" | GPU " << Number(latest_->gpuUtilPercent, 1) << L"%";
        if (latest_->gpuMemoryAvailable) text << L", GPU memory " <<
            Number(static_cast<double>(latest_->gpuMemoryUsedBytes) / (1024.0 * 1024.0 * 1024.0), 1) << L" GiB";
    } else {
        text << L" | GPU N/A";
    }
    text << L" | Down " << FormatRate(latest_->networkDownloadBytesPerSecond)
         << L"/s, Up " << FormatRate(latest_->networkUploadBytesPerSecond) << L"/s";
    return text.str();
}

void AppUi::SetDisplayMode(DisplayMode mode) {
    config_.displayMode = mode;
    configService_.Save(config_);
    ApplyMode();
}

void AppUi::ShowTrayMenu(POINT point) {
    HMENU menu = CreatePopupMenu();
    HMENU modes = CreatePopupMenu();
    AppendMenuW(modes, MF_STRING | (config_.displayMode == DisplayMode::Tray ? MF_CHECKED : 0),
                kTrayModeMenuId, L"托盘摘要");
    AppendMenuW(modes, MF_STRING | (config_.displayMode == DisplayMode::Taskbar ? MF_CHECKED : 0),
                kTaskbarModeMenuId, L"任务栏信息条");
    AppendMenuW(modes, MF_STRING | (config_.displayMode == DisplayMode::Hud ? MF_CHECKED : 0),
                kHudModeMenuId, L"桌面 HUD");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(modes), L"显示模式");
    AppendMenuW(menu, MF_GRAYED, 0, L"任务栏信息条：实验功能");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kSettingsMenuId, L"设置");
    AppendMenuW(menu, MF_STRING, kExitMenuId, L"退出");

    SetForegroundWindow(mainWindow_);
    const int command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, point.x, point.y, 0,
                                       mainWindow_, nullptr);
    DestroyMenu(menu);
    switch (command) {
        case kTrayModeMenuId:
            SetDisplayMode(DisplayMode::Tray);
            break;
        case kTaskbarModeMenuId:
            SetDisplayMode(DisplayMode::Taskbar);
            break;
        case kHudModeMenuId:
            SetDisplayMode(DisplayMode::Hud);
            break;
        case kSettingsMenuId:
            ShowSettings();
            break;
        case kExitMenuId:
            DestroyWindow(mainWindow_);
            break;
        default:
            break;
    }
}

void AppUi::ShowSettings() {
    if (settingsWindow_ == nullptr) {
        const UINT dpi = GetDpiForSystem();
        const DWORD style = WS_OVERLAPPEDWINDOW | WS_VSCROLL;
        RECT windowRect{0, 0, PixelsFromDip(kSettingsWidthDip, dpi),
                        PixelsFromDip(kSettingsContentHeightDip, dpi)};
        AdjustWindowRectExForDpi(&windowRect, style, FALSE, WS_EX_DLGMODALFRAME, dpi);
        settingsWindow_ = CreateWindowExW(WS_EX_DLGMODALFRAME, kSettingsClass, L"SysGlance 设置",
                                          style, CW_USEDEFAULT, CW_USEDEFAULT,
                                          windowRect.right - windowRect.left,
                                          windowRect.bottom - windowRect.top, nullptr, nullptr,
                                          instance_, this);
        if (settingsWindow_ == nullptr) return;
        BuildSettingsControls(settingsWindow_);
        ScaleSettingsControls(GetDpiForWindow(settingsWindow_));
    }
    settingsDraft_ = config_;
    PopulateDeviceSelectors();
    UpdateSettingsButtonState();
    ShowWindow(settingsWindow_, SW_SHOWNORMAL);
    FitSettingsWindowToWorkArea();
    UpdateSettingsScrollBar();
    SetForegroundWindow(settingsWindow_);
}

void AppUi::BuildSettingsControls(HWND hwnd) {
    auto label = [hwnd](const wchar_t* text, int x, int y, int width) {
        HWND control = CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE, x, y, width, 22, hwnd,
                                     nullptr, nullptr, nullptr);
        SetControlFont(control);
    };
    auto check = [hwnd](const wchar_t* text, int id, int x, int y, int width = 140) {
        HWND control = CreateWindowW(L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, x,
                                     y, width, 24, hwnd,
                                     reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr,
                                     nullptr);
        SetControlFont(control);
        return control;
    };

    label(L"显示模式", 24, 22, 120);
    modeCombo_ = CreateWindowW(L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                               180, 18, 340, 120, hwnd,
                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(kModeComboId)),
                               nullptr, nullptr);
    SendMessageW(modeCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"托盘摘要"));
    SendMessageW(modeCombo_, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(L"任务栏边缘信息卡（不占用任务栏）"));
    SendMessageW(modeCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"桌面 HUD"));
    SetControlFont(modeCombo_);

    label(L"刷新间隔", 24, 60, 120);
    intervalCombo_ = CreateWindowW(L"COMBOBOX", nullptr,
                                   WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 180, 56, 160, 120,
                                   hwnd,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIntervalComboId)),
                                   nullptr, nullptr);
    SendMessageW(intervalCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"500 ms"));
    SendMessageW(intervalCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"1000 ms"));
    SendMessageW(intervalCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"2000 ms"));
    SetControlFont(intervalCombo_);

    label(L"任务栏信息条为实验功能；推荐使用 HUD 或托盘。", 180, 78, 330);

    label(L"HUD 显示项（可分别开关）", 24, 102, 180);
    cpuCheck_ = check(L"CPU", kCpuCheckId, 210, 98, 100);
    memoryCheck_ = check(L"内存", kMemoryCheckId, 330, 98, 100);
    gpuCheck_ = check(L"GPU", kGpuCheckId, 210, 126, 90);
    networkCheck_ = check(L"网络", kNetworkCheckId, 310, 126, 80);
    networkArrowsCheck_ = check(L"显示上下行箭头", kNetworkArrowsCheckId, 390, 126, 150);

    label(L"内存显示方式", 24, 162, 150);
    memoryModeCombo_ = CreateWindowW(
        L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 180, 158, 220, 120,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kMemoryModeComboId)), nullptr,
        nullptr);
    SendMessageW(memoryModeCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"实际已用内存（GiB，不带单位）"));
    SendMessageW(memoryModeCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"内存使用率（百分比）"));
    SetControlFont(memoryModeCombo_);

    label(L"GPU 内存显示方式", 24, 198, 150);
    gpuMemoryModeCombo_ = CreateWindowW(
        L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 180, 194, 220, 120,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kGpuMemoryModeComboId)), nullptr,
        nullptr);
    SendMessageW(gpuMemoryModeCombo_, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(L"实际已用 GPU 内存（GiB）"));
    SendMessageW(gpuMemoryModeCombo_, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(L"已用 GPU 内存（百分比）"));
    SetControlFont(gpuMemoryModeCombo_);

    label(L"百分比精度", 24, 234, 150);
    percentPrecisionCombo_ = CreateWindowW(
        L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 180, 230, 220, 120,
        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPercentPrecisionComboId)), nullptr,
        nullptr);
    SendMessageW(percentPrecisionCombo_, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(L"显示一位小数"));
    SendMessageW(percentPrecisionCombo_, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(L"不显示小数"));
    SetControlFont(percentPrecisionCombo_);

    label(L"显示内容说明", 24, 260, 140);
    label(L"CPU   C8.0：系统整体 CPU 使用率。", 180, 256, 350);
    label(L"内存    8.0：已用 GiB；或 45.0：内存使用率。", 180, 278, 350);
    label(L"GPU   G8.0/0.5：利用率 / 已用 GPU 内存。", 180, 300, 350);
    label(L"网络  ↓  0.1MB  ↑  0.0KB：仅 KB/MB；≥100KB 用 MB。", 180, 322, 350);
    label(L"数值使用等宽固定槽位；HUD 宽度与字段位置不会随读数变化。", 180, 350, 350);

    label(L"HUD 预览", 24, 388, 120);
    settingsPreview_ = CreateWindowW(L"STATIC", nullptr, WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
                                     180, 378, 330, 46, hwnd,
                                     reinterpret_cast<HMENU>(static_cast<INT_PTR>(kSettingsPreviewId)),
                                     nullptr, nullptr);

    label(L"外观与 HUD", 24, 444, 120);
    lockedCheck_ = check(L"锁定 HUD", kLockedCheckId, 180, 440, 140);
    clickThroughCheck_ = check(L"鼠标穿透（自动锁定）", kClickThroughCheckId, 330, 440, 180);
    autoStartCheck_ = check(L"开机自动启动", kAutoStartCheckId, 180, 468, 140);

    label(L"HUD 透明度", 24, 508, 120);
    opacityEdit_ = CreateWindowW(L"EDIT", nullptr, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                                 180, 504, 70, 24, hwnd,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(kOpacityEditId)),
                                 nullptr, nullptr);
    SetControlFont(opacityEdit_);
    label(L"30 - 100", 260, 508, 100);

    label(L"字体大小", 24, 544, 120);
    fontSizeEdit_ = CreateWindowW(L"EDIT", nullptr, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                                  180, 540, 100, 24, hwnd,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(kFontSizeEditId)),
                                  nullptr, nullptr);
    SetControlFont(fontSizeEdit_);
    label(L"px", 290, 544, 40);

    label(L"HUD 尺寸", 24, 580, 120);
    hudWidthEdit_ = CreateWindowW(L"EDIT", nullptr, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                                  180, 576, 80, 24, hwnd,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(kHudWidthEditId)),
                                  nullptr, nullptr);
    hudHeightEdit_ = CreateWindowW(L"EDIT", nullptr, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                                   300, 576, 80, 24, hwnd,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(kHudHeightEditId)),
                                   nullptr, nullptr);
    SetControlFont(hudWidthEdit_);
    SetControlFont(hudHeightEdit_);
    label(L"×", 270, 580, 20);
    label(L"px", 384, 580, 40);

    label(L"外框粗细", 24, 620, 120);
    borderThicknessEdit_ = CreateWindowW(L"EDIT", nullptr, WS_CHILD | WS_VISIBLE | WS_BORDER,
                                         180, 616, 100, 24, hwnd,
                                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(kBorderThicknessEditId)),
                                         nullptr, nullptr);
    SetControlFont(borderThicknessEdit_);
    label(L"px（可填小数）", 290, 620, 130);

    label(L"配色预设", 24, 660, 120);
    colorPresetCombo_ = CreateWindowW(
        L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 180, 656, 220, 120, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kColorPresetComboId)), nullptr, nullptr);
    SendMessageW(colorPresetCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"夜橙深色（默认）"));
    SendMessageW(colorPresetCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"冰蓝浅色"));
    SendMessageW(colorPresetCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"翡翠深色"));
    SetControlFont(colorPresetCombo_);

    label(L"自定义颜色", 24, 700, 120);
    HWND borderColor = CreateWindowW(L"BUTTON", L"外框颜色", WS_CHILD | WS_VISIBLE,
                                     180, 696, 100, 26, hwnd,
                                     reinterpret_cast<HMENU>(static_cast<INT_PTR>(kBorderColorButtonId)),
                                     nullptr, nullptr);
    HWND textColor = CreateWindowW(L"BUTTON", L"文字颜色", WS_CHILD | WS_VISIBLE,
                                   290, 696, 100, 26, hwnd,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTextColorButtonId)),
                                   nullptr, nullptr);
    HWND backgroundColor = CreateWindowW(L"BUTTON", L"背景颜色", WS_CHILD | WS_VISIBLE,
                                         400, 696, 100, 26, hwnd,
                                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(kBackgroundColorButtonId)),
                                         nullptr, nullptr);
    SetControlFont(borderColor);
    SetControlFont(textColor);
    SetControlFont(backgroundColor);
    label(L"预设会覆盖三种颜色；任意颜色均可单独自定义。", 180, 726, 330);

    label(L"网络来源", 24, 764, 120);
    networkInterfaceCombo_ = CreateWindowW(L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 180, 760, 330, 160, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kNetworkInterfaceComboId)), nullptr, nullptr);
    SetControlFont(networkInterfaceCombo_);
    includeVirtualNetworkCheck_ = check(L"包含 VPN / 虚拟接口", kIncludeVirtualNetworkCheckId,
                                        180, 788, 210);

    label(L"GPU 来源", 24, 824, 120);
    gpuAdapterCombo_ = CreateWindowW(L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 180, 820, 330, 160, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kGpuAdapterComboId)), nullptr, nullptr);
    SetControlFont(gpuAdapterCombo_);
    label(L"默认汇总全部设备；GPU 内存可能包含共享内存。", 180, 848, 330);

    HWND recommended = CreateWindowW(L"BUTTON", L"恢复推荐 HUD", WS_CHILD | WS_VISIBLE,
                                     180, 884, 130, 30, hwnd,
                                     reinterpret_cast<HMENU>(static_cast<INT_PTR>(kRecommendedHudButtonId)),
                                     nullptr, nullptr);
    HWND lastGood = CreateWindowW(L"BUTTON", L"恢复上次可用布局", WS_CHILD | WS_VISIBLE,
                                  320, 884, 150, 30, hwnd,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(kLastGoodHudButtonId)),
                                  nullptr, nullptr);
    SetControlFont(recommended);
    SetControlFont(lastGood);

    previewWarning_ = CreateWindowW(L"STATIC", L"提示：右键拖动 HUD；左键不会触发操作。尺寸过小时内容可能裁切。",
                                    WS_CHILD | WS_VISIBLE, 24, 924, 500, 24, hwnd, nullptr, nullptr, nullptr);
    SetControlFont(previewWarning_);

    HWND apply = CreateWindowW(L"BUTTON", L"应用", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                               350, 956, 80, 30, hwnd,
                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(kApplyButtonId)),
                               nullptr, nullptr);
    HWND close = CreateWindowW(L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE,
                               440, 956, 80, 30, hwnd,
                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCloseButtonId)),
                               nullptr, nullptr);
    SetControlFont(apply);
    SetControlFont(close);
}

void AppUi::ScaleSettingsControls(UINT dpi) {
    if (settingsWindow_ == nullptr || dpi == 0) return;

    // Child positions are authored in 96-DPI logical pixels. Resetting the
    // scroll position first makes the current bounds suitable for proportional
    // scaling when the window moves between monitors.
    ScrollSettingsTo(0);
    const UINT oldDpi = settingsDpi_ == 0 ? 96 : settingsDpi_;
    if (oldDpi != dpi) {
        for (HWND child = GetWindow(settingsWindow_, GW_CHILD); child != nullptr;
             child = GetWindow(child, GW_HWNDNEXT)) {
            RECT bounds{};
            GetWindowRect(child, &bounds);
            MapWindowPoints(nullptr, settingsWindow_, reinterpret_cast<POINT*>(&bounds), 2);
            SetWindowPos(child, nullptr,
                         MulDiv(bounds.left, static_cast<int>(dpi), static_cast<int>(oldDpi)),
                         MulDiv(bounds.top, static_cast<int>(dpi), static_cast<int>(oldDpi)),
                         std::max(1, MulDiv(bounds.right - bounds.left, static_cast<int>(dpi),
                                            static_cast<int>(oldDpi))),
                         std::max(1, MulDiv(bounds.bottom - bounds.top, static_cast<int>(dpi),
                                            static_cast<int>(oldDpi))),
                         SWP_NOACTIVATE | SWP_NOZORDER);
        }
    }
    settingsDpi_ = dpi;
    settingsContentHeight_ = PixelsFromDip(kSettingsContentHeightDip, dpi);
    UpdateSettingsControlFont();
    UpdateSettingsScrollBar();
}

void AppUi::FitSettingsWindowToWorkArea() {
    if (settingsWindow_ == nullptr) return;

    const UINT dpi = GetDpiForWindow(settingsWindow_);
    const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(settingsWindow_, GWL_STYLE));
    const DWORD extendedStyle = static_cast<DWORD>(GetWindowLongPtrW(settingsWindow_, GWL_EXSTYLE));
    RECT desired{0, 0, PixelsFromDip(kSettingsWidthDip, dpi),
                 PixelsFromDip(kSettingsContentHeightDip, dpi)};
    AdjustWindowRectExForDpi(&desired, style, FALSE, extendedStyle, dpi);

    MONITORINFO monitor{sizeof(MONITORINFO)};
    GetMonitorInfoW(MonitorFromWindow(settingsWindow_, MONITOR_DEFAULTTONEAREST), &monitor);
    const int margin = PixelsFromDip(kSettingsWindowMarginDip, dpi);
    const int workWidth = static_cast<int>(monitor.rcWork.right - monitor.rcWork.left);
    const int workHeight = static_cast<int>(monitor.rcWork.bottom - monitor.rcWork.top);
    const int maxWidth = std::max(1, workWidth - margin);
    const int maxHeight = std::max(1, workHeight - margin);
    const int desiredWidth = static_cast<int>(desired.right - desired.left);
    const int desiredHeight = static_cast<int>(desired.bottom - desired.top);
    const int width = std::min(desiredWidth, maxWidth);
    const int height = std::min(desiredHeight, maxHeight);

    SetWindowPos(settingsWindow_, nullptr,
                 monitor.rcWork.left + (monitor.rcWork.right - monitor.rcWork.left - width) / 2,
                 monitor.rcWork.top + (monitor.rcWork.bottom - monitor.rcWork.top - height) / 2,
                 width, height, SWP_NOACTIVATE | SWP_NOZORDER);
}

void AppUi::UpdateSettingsScrollBar() {
    if (settingsWindow_ == nullptr || settingsContentHeight_ == 0) return;

    RECT client{};
    GetClientRect(settingsWindow_, &client);
    const int page = std::max(1, static_cast<int>(client.bottom - client.top));
    const int maximum = std::max(0, settingsContentHeight_ - page);
    if (settingsScrollPosition_ > maximum) {
        ScrollSettingsTo(maximum);
        return;
    }

    SCROLLINFO info{sizeof(SCROLLINFO)};
    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin = 0;
    info.nMax = std::max(0, settingsContentHeight_ - 1);
    info.nPage = static_cast<UINT>(page);
    info.nPos = settingsScrollPosition_;
    SetScrollInfo(settingsWindow_, SB_VERT, &info, TRUE);
    ShowScrollBar(settingsWindow_, SB_VERT, maximum > 0);
}

void AppUi::ScrollSettingsTo(int position) {
    if (settingsWindow_ == nullptr || settingsContentHeight_ == 0) return;

    RECT client{};
    GetClientRect(settingsWindow_, &client);
    const int clientHeight = static_cast<int>(client.bottom - client.top);
    const int maximum = std::max(0, settingsContentHeight_ - clientHeight);
    const int next = std::clamp(position, 0, maximum);
    if (next == settingsScrollPosition_) return;

    const int delta = settingsScrollPosition_ - next;
    ScrollWindowEx(settingsWindow_, 0, delta, nullptr, nullptr, nullptr, nullptr,
                   SW_ERASE | SW_INVALIDATE | SW_SCROLLCHILDREN);
    settingsScrollPosition_ = next;
    SCROLLINFO info{sizeof(SCROLLINFO)};
    info.fMask = SIF_POS;
    info.nPos = settingsScrollPosition_;
    SetScrollInfo(settingsWindow_, SB_VERT, &info, TRUE);
}

void AppUi::UpdateSettingsControlFont() {
    if (settingsWindow_ == nullptr) return;
    const int height = -std::max(1, MulDiv(9, static_cast<int>(settingsDpi_), 72));
    HFONT font = CreateFontW(height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    if (font == nullptr) return;
    HFONT previous = settingsFont_;
    settingsFont_ = font;
    for (HWND child = GetWindow(settingsWindow_, GW_CHILD); child != nullptr;
         child = GetWindow(child, GW_HWNDNEXT)) {
        SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(settingsFont_), TRUE);
    }
    if (previous != nullptr) DeleteObject(previous);
}

void AppUi::PopulateDeviceSelectors() {
    if (networkInterfaceCombo_ != nullptr) {
        SendMessageW(networkInterfaceCombo_, CB_RESETCONTENT, 0, 0);
        const int all = static_cast<int>(SendMessageW(networkInterfaceCombo_, CB_ADDSTRING, 0,
                                                      reinterpret_cast<LPARAM>(L"默认：聚合已连接物理网卡")));
        SendMessageW(networkInterfaceCombo_, CB_SETITEMDATA, all, 0);
        for (const auto& item : metrics_.NetworkInterfaces()) {
            std::wstring label = item.name + L" — " + item.description;
            if (!item.connected) label += L"（未连接）";
            else if (item.included) label += L"（当前纳入）";
            const int index = static_cast<int>(SendMessageW(networkInterfaceCombo_, CB_ADDSTRING, 0,
                                                             reinterpret_cast<LPARAM>(label.c_str())));
            SendMessageW(networkInterfaceCombo_, CB_SETITEMDATA, index,
                         static_cast<LPARAM>(item.luid));
        }
    }
    if (gpuAdapterCombo_ != nullptr) {
        SendMessageW(gpuAdapterCombo_, CB_RESETCONTENT, 0, 0);
        const int all = static_cast<int>(SendMessageW(gpuAdapterCombo_, CB_ADDSTRING, 0,
                                                      reinterpret_cast<LPARAM>(L"默认：聚合全部 GPU")));
        SendMessageW(gpuAdapterCombo_, CB_SETITEMDATA, all, 0);
        for (const auto& item : metrics_.GpuAdapters()) {
            const int index = static_cast<int>(SendMessageW(gpuAdapterCombo_, CB_ADDSTRING, 0,
                                                             reinterpret_cast<LPARAM>(item.name.c_str())));
            SendMessageW(gpuAdapterCombo_, CB_SETITEMDATA, index,
                         static_cast<LPARAM>(item.luid));
        }
    }
}

void AppUi::ReadSettingsControls(AppConfig& config) const {
    const int mode = static_cast<int>(SendMessageW(modeCombo_, CB_GETCURSEL, 0, 0));
    config.displayMode = mode == 1 ? DisplayMode::Taskbar : mode == 2 ? DisplayMode::Hud : DisplayMode::Tray;
    const int interval = static_cast<int>(SendMessageW(intervalCombo_, CB_GETCURSEL, 0, 0));
    config.refreshIntervalMs = interval == 0 ? 500 : interval == 2 ? 2000 : 1000;
    config.showCpu = SendMessageW(cpuCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    config.showMemory = SendMessageW(memoryCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    config.memoryShowPercent = SendMessageW(memoryModeCombo_, CB_GETCURSEL, 0, 0) == 1;
    config.gpuMemoryShowPercent = SendMessageW(gpuMemoryModeCombo_, CB_GETCURSEL, 0, 0) == 1;
    config.showGpu = SendMessageW(gpuCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    config.showNetwork = SendMessageW(networkCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    config.showNetworkArrows = SendMessageW(networkArrowsCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    config.showPercentDecimal = SendMessageW(percentPrecisionCombo_, CB_GETCURSEL, 0, 0) != 1;
    config.hudLocked = SendMessageW(lockedCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    config.hudClickThrough = SendMessageW(clickThroughCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    config.autoStart = SendMessageW(autoStartCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    config.includeVirtualNetworkInterfaces =
        SendMessageW(includeVirtualNetworkCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    const int network = static_cast<int>(SendMessageW(networkInterfaceCombo_, CB_GETCURSEL, 0, 0));
    config.selectedNetworkLuid = network >= 0
        ? static_cast<std::uint64_t>(SendMessageW(networkInterfaceCombo_, CB_GETITEMDATA, network, 0)) : 0;
    const int gpu = static_cast<int>(SendMessageW(gpuAdapterCombo_, CB_GETCURSEL, 0, 0));
    config.selectedGpuLuid = gpu >= 0
        ? static_cast<std::uint64_t>(SendMessageW(gpuAdapterCombo_, CB_GETITEMDATA, gpu, 0)) : 0;
    config.fontSize = ReadPositiveIntegerInput(fontSizeEdit_, config.fontSize, 1000);
    config.hudBorderThicknessTenths = ReadPositiveTenthsInput(borderThicknessEdit_, config.hudBorderThicknessTenths);
    wchar_t opacity[16]{};
    GetWindowTextW(opacityEdit_, opacity, _countof(opacity));
    config.hudOpacity = std::clamp(_wtoi(opacity), 30, 100);
    config.hudWidthDip = ReadPositiveIntegerInput(hudWidthEdit_, config.hudWidthDip);
    config.hudHeightDip = ReadPositiveIntegerInput(hudHeightEdit_, config.hudHeightDip);
    if (config.hudClickThrough) config.hudLocked = true;
    configService_.Normalize(config);
}

void AppUi::ResetSettingsDraft(bool recommended) {
    if (!settingsDraft_) settingsDraft_ = config_;
    if (recommended) *settingsDraft_ = configService_.RecommendedHud(*settingsDraft_);
    else configService_.LoadLastGoodHud(*settingsDraft_);
    UpdateSettingsButtonState();
    InvalidateSettingsPreview();
}

void AppUi::UpdateSettingsButtonState() {
    if (settingsWindow_ == nullptr) return;
    const AppConfig& draft = settingsDraft_ ? *settingsDraft_ : config_;
    SendMessageW(modeCombo_, CB_SETCURSEL, static_cast<int>(draft.displayMode), 0);
    const int intervalIndex = draft.refreshIntervalMs == 500 ? 0 : draft.refreshIntervalMs == 2000 ? 2 : 1;
    SendMessageW(intervalCombo_, CB_SETCURSEL, intervalIndex, 0);
    SendMessageW(cpuCheck_, BM_SETCHECK, draft.showCpu ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(memoryCheck_, BM_SETCHECK, draft.showMemory ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(memoryModeCombo_, CB_SETCURSEL, draft.memoryShowPercent ? 1 : 0, 0);
    SendMessageW(gpuMemoryModeCombo_, CB_SETCURSEL, draft.gpuMemoryShowPercent ? 1 : 0, 0);
    SendMessageW(gpuCheck_, BM_SETCHECK, draft.showGpu ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(networkCheck_, BM_SETCHECK, draft.showNetwork ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(networkArrowsCheck_, BM_SETCHECK,
                 draft.showNetworkArrows ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(percentPrecisionCombo_, CB_SETCURSEL, draft.showPercentDecimal ? 0 : 1, 0);
    SendMessageW(lockedCheck_, BM_SETCHECK, draft.hudLocked ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(clickThroughCheck_, BM_SETCHECK,
                 draft.hudClickThrough ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(autoStartCheck_, BM_SETCHECK, draft.autoStart ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(includeVirtualNetworkCheck_, BM_SETCHECK, draft.includeVirtualNetworkInterfaces ? BST_CHECKED : BST_UNCHECKED, 0);
    SetWindowTextW(opacityEdit_, std::to_wstring(draft.hudOpacity).c_str());
    SetWindowTextW(hudWidthEdit_, std::to_wstring(draft.hudWidthDip).c_str());
    SetWindowTextW(hudHeightEdit_, std::to_wstring(draft.hudHeightDip).c_str());
    SetWindowTextW(fontSizeEdit_, std::to_wstring(draft.fontSize).c_str());
    SetWindowTextW(borderThicknessEdit_,
                   Number(static_cast<double>(draft.hudBorderThicknessTenths) / 10.0, 1).c_str());
    SendMessageW(colorPresetCombo_, CB_SETCURSEL, draft.hudColorPreset, 0);
    auto selectLuid = [](HWND combo, std::uint64_t luid) {
        const int count = static_cast<int>(SendMessageW(combo, CB_GETCOUNT, 0, 0));
        for (int i = 0; i < count; ++i) if (static_cast<std::uint64_t>(SendMessageW(combo, CB_GETITEMDATA, i, 0)) == luid) {
            SendMessageW(combo, CB_SETCURSEL, i, 0); return;
        }
        SendMessageW(combo, CB_SETCURSEL, 0, 0);
    };
    selectLuid(networkInterfaceCombo_, draft.selectedNetworkLuid);
    selectLuid(gpuAdapterCombo_, draft.selectedGpuLuid);
}

void AppUi::ApplySettingsFromControls() {
    if (!settingsDraft_) settingsDraft_ = config_;
    ReadSettingsControls(*settingsDraft_);
    const int preset = static_cast<int>(SendMessageW(colorPresetCombo_, CB_GETCURSEL, 0, 0));
    if (preset >= 0) SetHudColorPreset(*settingsDraft_, preset);
    config_ = *settingsDraft_;
    configService_.Normalize(config_);
    configService_.Save(config_);
    SetAutoStart(config_.autoStart);
    metrics_.SetInterval(config_.refreshIntervalMs);
    metrics_.SetNetworkSelection(config_.selectedNetworkLuid,
                                 config_.includeVirtualNetworkInterfaces);
    metrics_.SetGpuSelection(config_.selectedGpuLuid);
    CreateTextFormat();
    ApplyHudStyle();
    ApplyMode();
    UpdateTrayTooltip();
    if (CanRenderHud(config_)) configService_.SaveLastGoodHud(config_);
    settingsDraft_ = config_;
    if (previewWarning_ != nullptr) {
        SetWindowTextW(previewWarning_, L"设置已应用并保存。右键拖动 HUD；左键不会触发操作。");
    }
    InvalidateSettingsPreview();
    return;

    const int mode = static_cast<int>(SendMessageW(modeCombo_, CB_GETCURSEL, 0, 0));
    config_.displayMode = mode == 1 ? DisplayMode::Taskbar : mode == 2 ? DisplayMode::Hud
                                                                         : DisplayMode::Tray;
    const int intervalIndex = static_cast<int>(SendMessageW(intervalCombo_, CB_GETCURSEL, 0, 0));
    config_.refreshIntervalMs = intervalIndex == 0 ? 500 : intervalIndex == 2 ? 2000 : 1000;
    config_.showCpu = SendMessageW(cpuCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    config_.showMemory = SendMessageW(memoryCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    config_.memoryShowPercent = SendMessageW(memoryModeCombo_, CB_GETCURSEL, 0, 0) == 1;
    config_.gpuMemoryShowPercent =
        SendMessageW(gpuMemoryModeCombo_, CB_GETCURSEL, 0, 0) == 1;
    config_.showGpu = SendMessageW(gpuCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    config_.showNetwork = SendMessageW(networkCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    config_.showNetworkArrows =
        SendMessageW(networkArrowsCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    config_.showPercentDecimal =
        SendMessageW(percentPrecisionCombo_, CB_GETCURSEL, 0, 0) != 1;
    config_.hudLocked = SendMessageW(lockedCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    config_.hudClickThrough = SendMessageW(clickThroughCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    if (config_.hudClickThrough) {
        config_.hudLocked = true;
    }
    config_.autoStart = SendMessageW(autoStartCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    config_.fontSize = ReadPositiveIntegerInput(fontSizeEdit_, config_.fontSize, 1000);
    config_.hudBorderThicknessTenths =
        ReadPositiveTenthsInput(borderThicknessEdit_, config_.hudBorderThicknessTenths);
    const int colorPresetIndex =
        static_cast<int>(SendMessageW(colorPresetCombo_, CB_GETCURSEL, 0, 0));
    if (colorPresetIndex >= 0) {
        SetHudColorPreset(config_, colorPresetIndex);
    }

    wchar_t opacityText[16]{};
    GetWindowTextW(opacityEdit_, opacityText, 16);
    config_.hudOpacity = std::clamp(_wtoi(opacityText), 30, 100);
    config_.hudWidthDip = ReadPositiveIntegerInput(hudWidthEdit_, config_.hudWidthDip);
    config_.hudHeightDip = ReadPositiveIntegerInput(hudHeightEdit_, config_.hudHeightDip);
    configService_.Save(config_);
    SetAutoStart(config_.autoStart);
    metrics_.SetInterval(config_.refreshIntervalMs);
    CreateTextFormat();
    ApplyHudStyle();
    ApplyMode();
    UpdateTrayTooltip();
}

void AppUi::EnsureRenderTarget(HWND hwnd) {
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget>* target =
        hwnd == taskbarWindow_ ? &taskbarRenderTarget_ : &hudRenderTarget_;
    if (*target || !d2dFactory_) return;
    RECT rect{};
    GetClientRect(hwnd, &rect);
    const UINT dpi = GetDpiForWindow(hwnd);
    d2dFactory_->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(),
                                     static_cast<FLOAT>(dpi), static_cast<FLOAT>(dpi)),
        D2D1::HwndRenderTargetProperties(hwnd,
                                         D2D1::SizeU(static_cast<UINT32>(std::max(1L, rect.right)),
                                                     static_cast<UINT32>(std::max(1L, rect.bottom)))),
        target->GetAddressOf());
}

void AppUi::RenderSurface(HWND hwnd, bool hud) {
    if (hwnd == nullptr || latest_ == nullptr) return;
    EnsureRenderTarget(hwnd);
    auto* target = hwnd == taskbarWindow_ ? taskbarRenderTarget_.Get() : hudRenderTarget_.Get();
    if (target == nullptr || !textFormat_) return;

    const D2D1_SIZE_F size = target->GetSize();
    const auto background = ToD2DColor(config_.hudBackgroundColor, hud ? 0.90f : 0.96f);
    const auto foreground = ToD2DColor(config_.hudTextColor, 1.0f);
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    target->CreateSolidColorBrush(foreground, brush.GetAddressOf());
    target->BeginDraw();
    target->Clear(background);
    const auto text = MetricsText(config_);
    constexpr float kHorizontalPadding = 8.0f;
    const auto bounds = D2D1::RectF(kHorizontalPadding, 0.0f,
                                    size.width - kHorizontalPadding, size.height);
    target->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()), textFormat_.Get(), bounds,
                      brush.Get());
    const HRESULT result = target->EndDraw();
    if (result == D2DERR_RECREATE_TARGET) {
        if (hud) hudRenderTarget_.Reset(); else taskbarRenderTarget_.Reset();
    }
}

LRESULT CALLBACK AppUi::MainWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    AppUi* app = reinterpret_cast<AppUi*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        app = static_cast<AppUi*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    return app != nullptr ? app->HandleMainMessage(hwnd, message, wParam, lParam)
                          : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK AppUi::SurfaceWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    AppUi* app = reinterpret_cast<AppUi*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        app = static_cast<AppUi*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    return app != nullptr ? app->HandleSurfaceMessage(hwnd, message, wParam, lParam)
                          : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK AppUi::SettingsWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    AppUi* app = reinterpret_cast<AppUi*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        app = static_cast<AppUi*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    return app != nullptr ? app->HandleSettingsMessage(hwnd, message, wParam, lParam)
                          : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT AppUi::HandleMainMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_DISPLAYCHANGE:
        case WM_SETTINGCHANGE:
            PositionTaskbarSurface();
            return 0;
        case kMetricsReadyMessage:
            latest_ = metrics_.Snapshot();
            if (config_.selectedGpuLuid != 0) {
                const auto adapters = metrics_.GpuAdapters();
                const bool found = std::any_of(adapters.begin(), adapters.end(), [this](const auto& item) {
                    return item.luid == config_.selectedGpuLuid;
                });
                if (!found) {
                    config_.selectedGpuLuid = 0;
                    metrics_.SetGpuSelection(0);
                    configService_.Save(config_);
                }
            }
            if (config_.selectedNetworkLuid != 0) {
                const auto interfaces = metrics_.NetworkInterfaces();
                if (ShouldFallbackToAggregateNetwork(config_.selectedNetworkLuid, interfaces)) {
                    config_.selectedNetworkLuid = 0;
                    metrics_.SetNetworkSelection(0, config_.includeVirtualNetworkInterfaces);
                    configService_.Save(config_);
                }
            }
            UpdateTrayTooltip();
            InvalidateRect(taskbarWindow_, nullptr, FALSE);
            InvalidateRect(hudWindow_, nullptr, FALSE);
            InvalidateRect(hudFrameWindow_, nullptr, FALSE);
            return 0;
        case kTrayMessage:
            if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
                POINT point{};
                GetCursorPos(&point);
                ShowTrayMenu(point);
            } else if (lParam == WM_LBUTTONDBLCLK) {
                ShowSettings();
            }
            return 0;
        case WM_QUERYENDSESSION:
            return TRUE;
        case WM_ENDSESSION:
            if (wParam) PostQuitMessage(0);
            return 0;
        case WM_DESTROY:
            if (!exiting_) {
                exiting_ = true;
                PostQuitMessage(0);
            }
            return 0;
        default:
            return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

LRESULT AppUi::HandleSurfaceMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    const bool hudContent = hwnd == hudWindow_;
    const bool hudFrame = hwnd == hudFrameWindow_;
    const bool hud = hudContent || hudFrame;
    switch (message) {
        case WM_PAINT:
            if (hudFrame) {
                RenderHudFrame(hwnd);
                return 0;
            }
            {
            PAINTSTRUCT paint{};
            BeginPaint(hwnd, &paint);
            RenderSurface(hwnd, hudContent);
            EndPaint(hwnd, &paint);
            return 0;
            }
        case WM_ERASEBKGND:
            return 1;
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_NCHITTEST:
            if (hud && !config_.hudLocked && !config_.hudClickThrough) return HTCLIENT;
            if (hud) return HTTRANSPARENT;
            return HTCLIENT;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
            // HUD left-clicks deliberately do nothing. Settings buttons remain
            // regular controls in their own window and still use the left button.
            if (hud) return 0;
            break;
        case WM_RBUTTONDOWN:
            if (hud && !config_.hudLocked && !config_.hudClickThrough) {
                GetCursorPos(&hudDragStartCursor_);
                GetWindowRect(hudFrameWindow_, &hudDragStartRect_);
                hudDragging_ = true;
                SetCapture(hwnd);
                return 0;
            }
            break;
        case WM_MOUSEMOVE:
            if (hud && hudDragging_ && GetCapture() == hwnd) {
                POINT cursor{};
                GetCursorPos(&cursor);
                MoveHudTo(hudDragStartRect_.left + cursor.x - hudDragStartCursor_.x,
                          hudDragStartRect_.top + cursor.y - hudDragStartCursor_.y);
                return 0;
            }
            break;
        case WM_RBUTTONUP:
            if (hud && hudDragging_) {
                hudDragging_ = false;
                if (GetCapture() == hwnd) {
                    ReleaseCapture();
                }
                SaveHudPlacement();
                return 0;
            }
            break;
        case WM_RBUTTONDBLCLK:
        case WM_CONTEXTMENU:
            if (hud) return 0;
            break;
        case WM_CAPTURECHANGED:
            if (hudDragging_) {
                hudDragging_ = false;
                SaveHudPlacement();
            }
            break;
        case WM_EXITSIZEMOVE:
            if (hud) {
                SaveHudPlacement();
            }
            return 0;
        case WM_DPICHANGED:
            if (hud) {
                if (hudFrame) {
                    config_.hudRect = *reinterpret_cast<const RECT*>(lParam);
                }
                if (hudContent) {
                    hudRenderTarget_.Reset();
                }
                PositionHudSurface();
                SaveHudPlacement();
                return 0;
            }
            break;
        case WM_DISPLAYCHANGE:
        case WM_SETTINGCHANGE:
            if (hud) {
                PositionHudSurface();
                return 0;
            }
            break;
        case WM_SIZE:
            if (hudContent) hudRenderTarget_.Reset();
            else if (!hudFrame) taskbarRenderTarget_.Reset();
            return 0;
        default:
            return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT AppUi::HandleSettingsMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_SIZE:
            UpdateSettingsScrollBar();
            return 0;
        case WM_DPICHANGED: {
            const auto* suggested = reinterpret_cast<const RECT*>(lParam);
            if (suggested != nullptr) {
                SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                             suggested->right - suggested->left, suggested->bottom - suggested->top,
                             SWP_NOACTIVATE | SWP_NOZORDER);
            }
            ScaleSettingsControls(HIWORD(wParam));
            FitSettingsWindowToWorkArea();
            return 0;
        }
        case WM_VSCROLL: {
            SCROLLINFO info{sizeof(SCROLLINFO)};
            info.fMask = SIF_ALL;
            GetScrollInfo(hwnd, SB_VERT, &info);
            int next = settingsScrollPosition_;
            const int line = PixelsFromDip(28.0f, settingsDpi_);
            switch (LOWORD(wParam)) {
                case SB_LINEUP: next -= line; break;
                case SB_LINEDOWN: next += line; break;
                case SB_PAGEUP: next -= static_cast<int>(info.nPage); break;
                case SB_PAGEDOWN: next += static_cast<int>(info.nPage); break;
                case SB_THUMBTRACK:
                case SB_THUMBPOSITION: next = info.nTrackPos; break;
                case SB_TOP: next = 0; break;
                case SB_BOTTOM: next = info.nMax; break;
                default: return 0;
            }
            ScrollSettingsTo(next);
            return 0;
        }
        case WM_MOUSEWHEEL:
            ScrollSettingsTo(settingsScrollPosition_ -
                             (GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA) *
                                 PixelsFromDip(48.0f, settingsDpi_));
            return 0;
        case WM_DRAWITEM: {
            const auto* draw = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
            if (draw != nullptr && draw->CtlID == kSettingsPreviewId) {
                DrawSettingsPreview(*draw);
                return TRUE;
            }
            break;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == kColorPresetComboId && HIWORD(wParam) == CBN_SELCHANGE &&
                settingsDraft_) {
                const int preset = static_cast<int>(SendMessageW(colorPresetCombo_, CB_GETCURSEL, 0, 0));
                if (preset >= 0) SetHudColorPreset(*settingsDraft_, preset);
                InvalidateSettingsPreview();
                return 0;
            }
            if (HIWORD(wParam) == EN_CHANGE || HIWORD(wParam) == CBN_SELCHANGE ||
                HIWORD(wParam) == BN_CLICKED) {
                if (settingsDraft_ && previewWarning_ != nullptr) {
                    AppConfig preview = *settingsDraft_;
                    ReadSettingsControls(preview);
                    SetWindowTextW(previewWarning_, CanRenderHud(preview)
                        ? L"右键拖动 HUD；左键不会触发操作。预览仅为草稿，应用后才会生效。"
                        : L"内容可能裁切：可自由保留该尺寸，或恢复推荐 HUD / 上次可用布局。 ");
                }
                InvalidateSettingsPreview();
            }
            switch (LOWORD(wParam)) {
                case kApplyButtonId:
                    ApplySettingsFromControls();
                    return 0;
                case kCloseButtonId:
                    settingsDraft_.reset();
                    ShowWindow(hwnd, SW_HIDE);
                    return 0;
                case kRecommendedHudButtonId:
                    ResetSettingsDraft(true);
                    return 0;
                case kLastGoodHudButtonId:
                    ResetSettingsDraft(false);
                    return 0;
                case kBorderColorButtonId:
                    if (!settingsDraft_) settingsDraft_ = config_;
                    ChooseHudColor(hwnd, settingsDraft_->hudBorderColor);
                    settingsDraft_->hudColorPreset = -1;
                    SendMessageW(colorPresetCombo_, CB_SETCURSEL, static_cast<WPARAM>(-1), 0);
                    InvalidateSettingsPreview();
                    return 0;
                case kTextColorButtonId:
                    if (!settingsDraft_) settingsDraft_ = config_;
                    ChooseHudColor(hwnd, settingsDraft_->hudTextColor);
                    settingsDraft_->hudColorPreset = -1;
                    SendMessageW(colorPresetCombo_, CB_SETCURSEL, static_cast<WPARAM>(-1), 0);
                    InvalidateSettingsPreview();
                    return 0;
                case kBackgroundColorButtonId:
                    if (!settingsDraft_) settingsDraft_ = config_;
                    ChooseHudColor(hwnd, settingsDraft_->hudBackgroundColor);
                    settingsDraft_->hudColorPreset = -1;
                    SendMessageW(colorPresetCombo_, CB_SETCURSEL, static_cast<WPARAM>(-1), 0);
                    InvalidateSettingsPreview();
                    return 0;
                default:
                    break;
            }
            break;
        case WM_CLOSE:
            settingsDraft_.reset();
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        case WM_DESTROY:
            settingsWindow_ = nullptr;
            settingsPreview_ = nullptr;
            if (settingsFont_ != nullptr) {
                DeleteObject(settingsFont_);
                settingsFont_ = nullptr;
            }
            settingsContentHeight_ = 0;
            settingsScrollPosition_ = 0;
            settingsDpi_ = 96;
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

}  // namespace sysglance
