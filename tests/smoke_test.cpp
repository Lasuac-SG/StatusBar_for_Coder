#include <QtTest>

class SmokeTest final : public QObject {
    Q_OBJECT

private slots:
    void testHarnessRuns() { QCOMPARE(2 + 2, 4); }
};

QTEST_GUILESS_MAIN(SmokeTest)
#include "smoke_test.moc"
