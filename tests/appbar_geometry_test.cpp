#include "platform/appbar.h"
#include "platform/single_instance.h"
#include "platform/tray_icon.h"
#include "platform/windows_shell_integration.h"

#include <QtTest>

#include <type_traits>

static_assert(!std::is_copy_constructible_v<Platform::AppBar>);
static_assert(!std::is_move_constructible_v<Platform::AppBar>);
static_assert(!std::is_copy_constructible_v<Platform::SingleInstance>);
static_assert(!std::is_move_constructible_v<Platform::SingleInstance>);
static_assert(!std::is_copy_constructible_v<Platform::TrayIcon>);
static_assert(!std::is_move_constructible_v<Platform::TrayIcon>);
static_assert(!std::is_copy_constructible_v<Platform::WindowsShellIntegration>);
static_assert(!std::is_move_constructible_v<Platform::WindowsShellIntegration>);

class AppBarGeometryTest final : public QObject {
    Q_OBJECT

private slots:
    void inactiveAppBarHasNoCallbackMessage();
    void preservesShellTopAndMonitorWidth();
    void supportsNegativeMonitorOrigin();
    void preservesExactEdgeContract();
};

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

QTEST_APPLESS_MAIN(AppBarGeometryTest)

#include "appbar_geometry_test.moc"
