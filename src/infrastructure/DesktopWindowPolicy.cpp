#include "infrastructure/DesktopWindowPolicy.h"

#include <QGuiApplication>
#include <QWidget>

#ifdef Q_OS_WIN
#include <windows.h>

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif
#endif

namespace zhu_screen_pet {

bool DesktopWindowPolicy::apply(QWidget* window, const DesktopWindowOptions& options,
                                QString* errorMessage)
{
    if (window == nullptr) {
        if (errorMessage) *errorMessage = QStringLiteral("window is null");
        return false;
    }
    window->setWindowFlag(Qt::FramelessWindowHint, options.frameless);
    window->setWindowFlag(Qt::WindowStaysOnTopHint, options.alwaysOnTop);
    window->setWindowFlag(Qt::Tool, !options.showInTaskbar);
    window->setWindowFlag(Qt::WindowDoesNotAcceptFocus, !options.acceptFocus);
    window->setAttribute(Qt::WA_TranslucentBackground, options.translucentBackground);
    window->setAttribute(Qt::WA_ShowWithoutActivating, !options.acceptFocus);
    window->setAttribute(Qt::WA_QuitOnClose, false);
    if (!setMouseInputTransparent(window, options.mouseInputTransparent, errorMessage)) {
        return false;
    }
    return setExcludedFromCapture(window, options.excludeFromCapture, errorMessage);
}

bool DesktopWindowPolicy::setMouseInputTransparent(QWidget* window, bool enabled,
                                                   QString* errorMessage)
{
    if (window == nullptr) {
        if (errorMessage) *errorMessage = QStringLiteral("window is null");
        return false;
    }
    window->setAttribute(Qt::WA_TransparentForMouseEvents, enabled);
    // 在首次 show() 前通过 Qt 窗口标志记录策略，不要调用 winId()
    // 强制创建 HWND。构造期创建原生窗口会同步派发 Windows 事件。
    window->setWindowFlag(Qt::WindowTransparentForInput, enabled);
#ifdef Q_OS_WIN
    if (!window->isWindow() || QGuiApplication::platformName() != QStringLiteral("windows")) {
        return true;
    }
    const WId nativeId = window->internalWinId();
    if (nativeId == 0) return true;
    HWND handle = reinterpret_cast<HWND>(nativeId);
    SetLastError(ERROR_SUCCESS);
    LONG_PTR style = GetWindowLongPtrW(handle, GWL_EXSTYLE);
    if (style == 0 && GetLastError() != ERROR_SUCCESS) {
        if (errorMessage) *errorMessage = QStringLiteral("GetWindowLongPtr failed: %1").arg(GetLastError());
        return false;
    }
    if (enabled) style |= WS_EX_TRANSPARENT | WS_EX_LAYERED;
    else style &= ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT);
    SetLastError(ERROR_SUCCESS);
    const LONG_PTR previous = SetWindowLongPtrW(handle, GWL_EXSTYLE, style);
    if (previous == 0 && GetLastError() != ERROR_SUCCESS) {
        if (errorMessage) *errorMessage = QStringLiteral("SetWindowLongPtr failed: %1").arg(GetLastError());
        return false;
    }
    SetWindowPos(handle, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
#else
    Q_UNUSED(errorMessage);
#endif
    return true;
}

bool DesktopWindowPolicy::setExcludedFromCapture(QWidget* window, bool excluded,
                                                  QString* errorMessage)
{
    if (window == nullptr) {
        if (errorMessage) *errorMessage = QStringLiteral("window is null");
        return false;
    }
#ifdef Q_OS_WIN
    if (!window->isWindow() || QGuiApplication::platformName() != QStringLiteral("windows")) {
        return true;
    }
    // 截图亲和性只能对已创建的 HWND 生效。此处不强制创建原生
    // 窗口；CaptureUiController 会在 Show/WinIdChange 后再次应用。
    const WId nativeId = window->internalWinId();
    if (nativeId == 0) return true;
    const HWND handle = reinterpret_cast<HWND>(nativeId);
    const DWORD affinity = excluded ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE;
    SetLastError(ERROR_SUCCESS);
    if (SetWindowDisplayAffinity(handle, affinity)) return true;

    const DWORD exclusionError = GetLastError();
    if (excluded) {
        SetLastError(ERROR_SUCCESS);
        if (SetWindowDisplayAffinity(handle, WDA_MONITOR)) return true;
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                "SetWindowDisplayAffinity failed: exclude=%1, monitor=%2")
                .arg(exclusionError)
                .arg(GetLastError());
        }
        return false;
    }
    if (errorMessage) {
        *errorMessage = QStringLiteral("SetWindowDisplayAffinity failed: %1")
            .arg(exclusionError);
    }
    return false;
#else
    Q_UNUSED(excluded);
    Q_UNUSED(errorMessage);
    return true;
#endif
}

} // namespace zhu_screen_pet
