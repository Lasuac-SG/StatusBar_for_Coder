#include "platform/tray_icon.h"

#include <QtTest>

#include <type_traits>

static_assert(!std::is_copy_constructible_v<Platform::TrayIcon>);
static_assert(!std::is_copy_assignable_v<Platform::TrayIcon>);
static_assert(!std::is_move_constructible_v<Platform::TrayIcon>);
static_assert(!std::is_move_assignable_v<Platform::TrayIcon>);

class SmokeTest final : public QObject {
    Q_OBJECT

private slots:
    void testHarnessRuns() { QCOMPARE(2 + 2, 4); }
};

QTEST_GUILESS_MAIN(SmokeTest)
#include "smoke_test.moc"
