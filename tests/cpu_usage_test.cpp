#include "platform/cpu_usage.h"

#include <QtTest>

#include <limits>

using Platform::CpuTimes;
using Platform::calculateCpuUsage;

Q_DECLARE_METATYPE(CpuTimes)

class CpuUsageTest final : public QObject {
    Q_OBJECT

private slots:
    void calculatesFiftyPercent()
    {
        QCOMPARE(calculateCpuUsage({100, 200, 300}, {150, 300, 300}), 50);
    }

    void calculatesFullyIdle()
    {
        QCOMPARE(calculateCpuUsage({100, 200, 300}, {200, 300, 300}), 0);
    }

    void calculatesLowUtilizationWithoutCancellation()
    {
        QCOMPARE(calculateCpuUsage({0, 0, 0}, {98, 100, 0}), 2);
    }

    void rejectsZeroTotalDelta()
    {
        QVERIFY(!calculateCpuUsage({100, 200, 300}, {100, 200, 300}).has_value());
    }

    void rejectsAnyCounterRegression_data()
    {
        QTest::addColumn<CpuTimes>("current");

        QTest::newRow("idle") << CpuTimes{99, 200, 300};
        QTest::newRow("kernel") << CpuTimes{100, 199, 300};
        QTest::newRow("user") << CpuTimes{100, 200, 299};
    }

    void rejectsAnyCounterRegression()
    {
        QFETCH(CpuTimes, current);
        QVERIFY(!calculateCpuUsage({100, 200, 300}, current).has_value());
    }

    void clampsResultsToBounds()
    {
        QCOMPARE(calculateCpuUsage({0, 0, 0}, {0, 1, 0}), 100);
        QCOMPARE(calculateCpuUsage({0, 0, 0}, {10, 1, 0}), 0);

        const auto maximum = std::numeric_limits<std::uint64_t>::max();
        QCOMPARE(
            calculateCpuUsage({0, 0, 0}, {maximum, maximum, maximum}),
            50);
    }
};

QTEST_GUILESS_MAIN(CpuUsageTest)
#include "cpu_usage_test.moc"
