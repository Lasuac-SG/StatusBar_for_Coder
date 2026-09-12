#include "platform/appbar.h"
#include "platform/single_instance.h"
#include "platform/tray_icon.h"
#include "platform/windows_shell_integration.h"

#include <QtTest>

#include <QCoreApplication>
#include <QStandardPaths>

#include <shellapi.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<Platform::AppBar>);
static_assert(!std::is_move_constructible_v<Platform::AppBar>);
static_assert(!std::is_copy_constructible_v<Platform::SingleInstance>);
static_assert(!std::is_move_constructible_v<Platform::SingleInstance>);
static_assert(!std::is_copy_constructible_v<Platform::TrayIcon>);
static_assert(!std::is_move_constructible_v<Platform::TrayIcon>);
static_assert(!std::is_copy_constructible_v<Platform::WindowsShellIntegration>);
static_assert(!std::is_move_constructible_v<Platform::WindowsShellIntegration>);

namespace {

template <typename T, std::size_t Capacity>
struct CallLog final {
    std::array<T, Capacity> values{};
    std::size_t size{};

    void clear() noexcept { size = 0; }
    void append(const T value) noexcept { values[size++] = value; }
};

CallLog<DWORD, 32> appBarCalls;
bool failAppBarRegistration{};
struct TrayCall final {
    DWORD message{};
    UINT flags{};
    UINT iconId{};
    UINT callbackMessage{};
    UINT version{};
};

CallLog<TrayCall, 32> trayCalls;
DWORD failingTrayCall{};
int remainingTrayFailures{};

UINT_PTR WINAPI fakeAppBarMessage(const DWORD message, PAPPBARDATA) noexcept
{
    appBarCalls.append(message);
    return message == ABM_NEW && !failAppBarRegistration ? TRUE : FALSE;
}

BOOL WINAPI fakeNotifyIcon(const DWORD message, PNOTIFYICONDATAW data) noexcept
{
    trayCalls.append({
        message,
        data == nullptr ? 0U : data->uFlags,
        data == nullptr ? 0U : data->uID,
        data == nullptr ? 0U : data->uCallbackMessage,
        data == nullptr ? 0U : data->uVersion,
    });
    if (message == failingTrayCall && remainingTrayFailures > 0) {
        --remainingTrayFailures;
        return FALSE;
    }
    return TRUE;
}

HRESULT WINAPI fakeNotifyIconRect(const NOTIFYICONIDENTIFIER*, RECT*) noexcept
{
    return E_FAIL;
}

HICON WINAPI fakeLoadIcon(HINSTANCE, LPCWSTR) noexcept
{
    return reinterpret_cast<HICON>(1);
}

class NativeWindow final {
public:
    NativeWindow()
        : window_(CreateWindowExW(
              WS_EX_TOOLWINDOW,
              L"STATIC",
              L"",
              WS_POPUP,
              0,
              0,
              16,
              16,
              nullptr,
              nullptr,
              GetModuleHandleW(nullptr),
              nullptr))
    {
    }

    ~NativeWindow()
    {
        if (window_ != nullptr) {
            DestroyWindow(window_);
        }
    }

    NativeWindow(const NativeWindow&) = delete;
    NativeWindow& operator=(const NativeWindow&) = delete;

    [[nodiscard]] HWND get() const noexcept { return window_; }

private:
    HWND window_{};
};

Platform::AppBarApi fakeAppBarApi() noexcept
{
    return {fakeAppBarMessage};
}

Platform::TrayIconApi fakeTrayApi() noexcept
{
    return {fakeNotifyIcon, fakeNotifyIconRect, fakeLoadIcon};
}

std::wstring uniqueMutexName(const void* const identity)
{
    return L"Local\\StatusBarForCoder.Test."
        + std::to_wstring(GetCurrentProcessId()) + L"."
        + std::to_wstring(reinterpret_cast<std::uintptr_t>(identity));
}

} // namespace

class AppBarGeometryTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void inactiveAppBarHasNoCallbackMessage();
    void preservesShellTopAndMonitorWidth();
    void supportsNegativeMonitorOrigin();
    void preservesExactEdgeContract();
    void appBarUsesCompleteProtocol();
    void appBarRecoveryChecksNewRegistration();
    void shellOwnsSingleInstance();
    void coalescesDeferredCalls();
    void displayChangesAreDeferredAndCoalesced();
    void shutdownCancelsDeferredDisplayChange();
    void decodesVersionFourTrayEvents();
    void trayNotificationFailureRemainsRecoverable_data();
    void trayNotificationFailureRemainsRecoverable();
    void taskbarCreatedRetriesUnavailableTray();
};

void AppBarGeometryTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void AppBarGeometryTest::inactiveAppBarHasNoCallbackMessage()
{
    const Platform::AppBar appBar;

    QCOMPARE(appBar.callbackMessage(), 0U);
}

void AppBarGeometryTest::preservesShellTopAndMonitorWidth()
{
    const RECT monitor{120, 80, 2040, 1160};
    const RECT shellAdjusted{135, 112, 1990, 152};

    const RECT result = Platform::topAppBarRect(monitor, shellAdjusted, 48);

    QCOMPARE(result.left, 120L);
    QCOMPARE(result.top, 112L);
    QCOMPARE(result.right, 2040L);
    QCOMPARE(result.bottom, 160L);
}

void AppBarGeometryTest::supportsNegativeMonitorOrigin()
{
    const RECT monitor{-1920, -240, 0, 840};
    const RECT shellAdjusted{-1910, -216, -10, -176};

    const RECT result = Platform::topAppBarRect(monitor, shellAdjusted, 40);

    QCOMPARE(result.left, -1920L);
    QCOMPARE(result.top, -216L);
    QCOMPARE(result.right, 0L);
    QCOMPARE(result.bottom, -176L);
}

void AppBarGeometryTest::preservesExactEdgeContract()
{
    const RECT monitor{-10, 20, -10, 20};
    const RECT shellAdjusted{99, 25, 101, 25};

    const RECT zeroHeight = Platform::topAppBarRect(monitor, shellAdjusted, 0);
    QCOMPARE(zeroHeight.left, -10L);
    QCOMPARE(zeroHeight.top, 25L);
    QCOMPARE(zeroHeight.right, -10L);
    QCOMPARE(zeroHeight.bottom, 25L);

    const RECT negativeHeight = Platform::topAppBarRect(monitor, shellAdjusted, -5);
    QCOMPARE(negativeHeight.bottom, 20L);
}

void AppBarGeometryTest::appBarUsesCompleteProtocol()
{
    NativeWindow window;
    QVERIFY(window.get() != nullptr);
    appBarCalls.clear();
    failAppBarRegistration = false;

    Platform::AppBar appBar(fakeAppBarApi());
    QVERIFY(appBar.initialize(window.get(), 40).hasValue());
    appBar.notifyActivated();
    appBar.notifyWindowPosChanged();
    QVERIFY(appBar.recoverAfterShellRestart().hasValue());
    appBar.shutdown();

    const std::array expected{
        DWORD{ABM_NEW}, DWORD{ABM_QUERYPOS}, DWORD{ABM_SETPOS},
        DWORD{ABM_ACTIVATE}, DWORD{ABM_WINDOWPOSCHANGED}, DWORD{ABM_REMOVE},
        DWORD{ABM_NEW}, DWORD{ABM_QUERYPOS}, DWORD{ABM_SETPOS}, DWORD{ABM_REMOVE}};
    QCOMPARE(appBarCalls.size, expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index) {
        QCOMPARE(appBarCalls.values[index], expected[index]);
    }
}

void AppBarGeometryTest::appBarRecoveryChecksNewRegistration()
{
    NativeWindow window;
    QVERIFY(window.get() != nullptr);
    appBarCalls.clear();
    failAppBarRegistration = false;

    Platform::AppBar appBar(fakeAppBarApi());
    QVERIFY(appBar.initialize(window.get(), 40).hasValue());
    appBarCalls.clear();
    failAppBarRegistration = true;

    QVERIFY(!appBar.recoverAfterShellRestart().hasValue());
    QCOMPARE(appBarCalls.size, std::size_t{2});
    QCOMPARE(appBarCalls.values[0], DWORD{ABM_REMOVE});
    QCOMPARE(appBarCalls.values[1], DWORD{ABM_NEW});
    failAppBarRegistration = false;
}

void AppBarGeometryTest::shellOwnsSingleInstance()
{
    const std::wstring name = uniqueMutexName(this);
    Platform::WindowsShellIntegration first;
    Platform::WindowsShellIntegration second;

    QVERIFY(first.acquireSingleInstance(name.c_str()).hasValue());
    QVERIFY(!first.alreadyRunning());
    QVERIFY(second.acquireSingleInstance(name.c_str()).hasValue());
    QVERIFY(second.alreadyRunning());
}

void AppBarGeometryTest::coalescesDeferredCalls()
{
    Platform::Detail::CoalescedCall call;
    const auto first = call.request();
    QVERIFY(first.has_value());
    QVERIFY(!call.request().has_value());
    QVERIFY(call.consume(*first));
    QVERIFY(!call.consume(*first));

    const auto stale = call.request();
    QVERIFY(stale.has_value());
    call.cancel();
    QVERIFY(!call.consume(*stale));
    QVERIFY(call.request().has_value());
}

void AppBarGeometryTest::displayChangesAreDeferredAndCoalesced()
{
    NativeWindow window;
    QVERIFY(window.get() != nullptr);
    appBarCalls.clear();
    trayCalls.clear();
    failAppBarRegistration = false;
    failingTrayCall = 0;
    remainingTrayFailures = 0;

    Platform::WindowsShellIntegration shell(fakeAppBarApi(), fakeTrayApi());
    QVERIFY(shell.initialize(window.get(), 40, [] {}).hasValue());
    appBarCalls.clear();

    MSG message{};
    message.hwnd = window.get();
    message.message = WM_ACTIVATE;
    QVERIFY(!shell.nativeEventFilter({}, &message, nullptr));
    message.message = WM_WINDOWPOSCHANGED;
    QVERIFY(!shell.nativeEventFilter({}, &message, nullptr));
    QCOMPARE(appBarCalls.size, std::size_t{2});
    QCOMPARE(appBarCalls.values[0], DWORD{ABM_ACTIVATE});
    QCOMPARE(appBarCalls.values[1], DWORD{ABM_WINDOWPOSCHANGED});
    appBarCalls.clear();

    message.message = WM_DISPLAYCHANGE;
    QVERIFY(!shell.nativeEventFilter({}, &message, nullptr));
    message.message = WM_DPICHANGED;
    QVERIFY(!shell.nativeEventFilter({}, &message, nullptr));
    QCOMPARE(appBarCalls.size, std::size_t{0});

    QTRY_COMPARE(appBarCalls.size, std::size_t{2});
    QCOMPARE(appBarCalls.values[0], DWORD{ABM_QUERYPOS});
    QCOMPARE(appBarCalls.values[1], DWORD{ABM_SETPOS});
    shell.shutdown();
}

void AppBarGeometryTest::decodesVersionFourTrayEvents()
{
    constexpr UINT iconId = 7;
    const auto mouse = Platform::Detail::decodeTrayNotification(
        MAKEWPARAM(static_cast<WORD>(-20), static_cast<WORD>(30)),
        MAKELPARAM(WM_RBUTTONUP, iconId),
        iconId);
    QVERIFY(mouse.has_value());
    QCOMPARE(mouse->notification, UINT{WM_RBUTTONUP});
    QCOMPARE(mouse->point.x, -20L);
    QCOMPARE(mouse->point.y, 30L);
    QVERIFY(!mouse->needsIconRect);

    const auto keyboard = Platform::Detail::decodeTrayNotification(
        MAKEWPARAM(static_cast<WORD>(-1), static_cast<WORD>(-1)),
        MAKELPARAM(NIN_KEYSELECT, iconId),
        iconId);
    QVERIFY(keyboard.has_value());
    QVERIFY(keyboard->needsIconRect);

    QVERIFY(!Platform::Detail::decodeTrayNotification(
                 0, MAKELPARAM(WM_RBUTTONUP, iconId + 1), iconId)
                 .has_value());
    QVERIFY(!Platform::Detail::decodeTrayNotification(
                 0, MAKELPARAM(WM_LBUTTONUP, iconId), iconId)
                 .has_value());
}

void AppBarGeometryTest::shutdownCancelsDeferredDisplayChange()
{
    NativeWindow window;
    QVERIFY(window.get() != nullptr);
    appBarCalls.clear();
    trayCalls.clear();
    failAppBarRegistration = false;
    failingTrayCall = 0;
    remainingTrayFailures = 0;

    {
        Platform::WindowsShellIntegration shell(fakeAppBarApi(), fakeTrayApi());
        QVERIFY(shell.initialize(window.get(), 40, [] {}).hasValue());

        MSG message{};
        message.hwnd = window.get();
        message.message = WM_DISPLAYCHANGE;
        QVERIFY(!shell.nativeEventFilter({}, &message, nullptr));
        shell.shutdown();
    }

    appBarCalls.clear();
    QCoreApplication::processEvents();
    QCOMPARE(appBarCalls.size, std::size_t{0});
}

void AppBarGeometryTest::trayNotificationFailureRemainsRecoverable_data()
{
    QTest::addColumn<quint32>("failure");
    QTest::newRow("NIM_ADD") << quint32{NIM_ADD};
    QTest::newRow("NIM_SETVERSION") << quint32{NIM_SETVERSION};
}

void AppBarGeometryTest::trayNotificationFailureRemainsRecoverable()
{
    QFETCH(quint32, failure);
    trayCalls.clear();
    failingTrayCall = failure;
    remainingTrayFailures = 1;

    Platform::TrayIcon tray(fakeTrayApi());
    QVERIFY(!tray.initialize([] {}).hasValue());
    QVERIFY(tray.taskbarCreatedMessage() != 0);
    QCOMPARE(trayCalls.values[0].message, DWORD{NIM_ADD});
    QVERIFY((trayCalls.values[0].flags & NIF_SHOWTIP) != 0);
    QCOMPARE(trayCalls.values[0].iconId, UINT{1});
    QVERIFY(trayCalls.values[0].callbackMessage != 0);

    failingTrayCall = 0;
    QVERIFY(tray.recoverAfterShellRestart().hasValue());
    QCOMPARE(trayCalls.values[trayCalls.size - 2].message, DWORD{NIM_ADD});
    QCOMPARE(trayCalls.values[trayCalls.size - 1].message, DWORD{NIM_SETVERSION});
    QCOMPARE(trayCalls.values[trayCalls.size - 1].version, UINT{NOTIFYICON_VERSION_4});
    tray.shutdown();
}

void AppBarGeometryTest::taskbarCreatedRetriesUnavailableTray()
{
    NativeWindow window;
    QVERIFY(window.get() != nullptr);
    appBarCalls.clear();
    trayCalls.clear();
    failAppBarRegistration = false;
    failingTrayCall = NIM_ADD;
    remainingTrayFailures = 1;

    Platform::WindowsShellIntegration shell(fakeAppBarApi(), fakeTrayApi());
    QVERIFY(shell.initialize(window.get(), 40, [] {}).hasValue());
    trayCalls.clear();
    failingTrayCall = 0;

    MSG message{};
    message.hwnd = window.get();
    message.message = RegisterWindowMessageW(L"TaskbarCreated");
    QVERIFY(message.message != 0);
    QVERIFY(!shell.nativeEventFilter({}, &message, nullptr));
    QCOMPARE(trayCalls.size, std::size_t{2});
    QCOMPARE(trayCalls.values[0].message, DWORD{NIM_ADD});
    QCOMPARE(trayCalls.values[1].message, DWORD{NIM_SETVERSION});
    shell.shutdown();
}

QTEST_GUILESS_MAIN(AppBarGeometryTest)

#include "appbar_geometry_test.moc"
