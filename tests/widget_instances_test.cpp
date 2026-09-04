#include "core/widget_config.h"
#include "platform/cpu_data_source.h"
#include "platform/cpu_service.h"
#include "widgets/clock/clock_view_model.h"
#include "widgets/cpu/cpu_view_model.h"

#include <QSignalSpy>
#include <QTimer>
#include <QtTest>

#include <optional>
#include <utility>
#include <vector>

namespace {

class CountingCpuDataSource final : public Platform::CpuDataSource {
public:
    std::vector<std::optional<Platform::CpuTimes>> times;
    std::vector<std::optional<double>> ratios;
    std::optional<Platform::CpuTopology> cpuTopology{
        Platform::CpuTopology{8, 16, 4000}};
    int timesCalls{};
    int ratioCalls{};
    int topologyCalls{};

    std::optional<Platform::CpuTimes> sampleTimes() noexcept override
    {
        ++timesCalls;
        if (timesIndex >= times.size()) {
            return std::nullopt;
        }
        return times[timesIndex++];
    }

    std::optional<double> samplePerformanceRatio() noexcept override
    {
        ++ratioCalls;
        if (ratioIndex >= ratios.size()) {
            return std::nullopt;
        }
        return ratios[ratioIndex++];
    }

    std::optional<Platform::CpuTopology> topology() noexcept override
    {
        ++topologyCalls;
        return cpuTopology;
    }

private:
    std::size_t timesIndex{};
    std::size_t ratioIndex{};
};

std::unique_ptr<CountingCpuDataSource> sourceWith(
    std::vector<std::optional<Platform::CpuTimes>> times,
    std::vector<std::optional<double>> ratios = {})
{
    auto source = std::make_unique<CountingCpuDataSource>();
    source->times = std::move(times);
    source->ratios = std::move(ratios);
    return source;
}

} // namespace

class WidgetInstancesTest final : public QObject {
    Q_OBJECT

private slots:
    void sharedCpuServiceSamplesOnceForTwoViewModels()
    {
        auto source = sourceWith(
            {{Platform::CpuTimes{100, 200, 300}},
             {Platform::CpuTimes{150, 300, 300}}},
            {{1.0}, {0.5}});
        auto* const counts = source.get();
        Platform::CpuService service(std::move(source));
        service.sampleNow();

        Widgets::CpuViewModel first(
            Core::WidgetConfig{"cpu-left", "Cpu", 0, QJsonObject{{"label", "left"}}},
            service);
        Widgets::CpuViewModel second(
            Core::WidgetConfig{"cpu-right", "Cpu", 2, QJsonObject{{"label", "right"}}},
            service);
        QVERIFY(static_cast<QObject*>(&first) != static_cast<QObject*>(&second));
        QCOMPARE(first.instanceId(), QString("cpu-left"));
        QCOMPARE(second.instanceId(), QString("cpu-right"));

        QSignalSpy firstCpuChanged(&first, &Widgets::CpuViewModel::cpuPercentChanged);
        QSignalSpy secondCpuChanged(&second, &Widgets::CpuViewModel::cpuPercentChanged);
        const int timesBefore = counts->timesCalls;
        const int ratiosBefore = counts->ratioCalls;

        service.sampleNow();

        QCOMPARE(counts->timesCalls, timesBefore + 1);
        QCOMPARE(counts->ratioCalls, ratiosBefore + 1);
        QCOMPARE(counts->topologyCalls, 1);
        QCOMPARE(first.cpuPercent(), 50);
        QCOMPARE(second.cpuPercent(), 50);
        QCOMPARE(first.currentFrequencyMHz(), 2000);
        QCOMPARE(second.currentFrequencyMHz(), 2000);
        QCOMPARE(first.history(), second.history());
        QCOMPARE(firstCpuChanged.count(), 1);
        QCOMPARE(secondCpuChanged.count(), 1);

        static_cast<void>(first.cpuPercent());
        static_cast<void>(second.currentFrequencyMHz());
        static_cast<void>(first.history());
        QCOMPARE(counts->timesCalls, timesBefore + 1);
        QCOMPARE(counts->ratioCalls, ratiosBefore + 1);
    }

    void cpuViewModelsOwnNoSamplingObjectsAndForwardServiceSignals()
    {
        auto source = sourceWith(
            {{Platform::CpuTimes{0, 0, 0}}, {Platform::CpuTimes{25, 100, 0}}},
            {{1.0}, {0.75}});
        Platform::CpuService service(std::move(source));
        Widgets::CpuViewModel first(Core::WidgetConfig{"one", "Cpu", 0, {}}, service);
        Widgets::CpuViewModel second(Core::WidgetConfig{"two", "Cpu", 2, {}}, service);

        QCOMPARE(first.findChildren<QTimer*>().size(), 0);
        QCOMPARE(second.findChildren<QTimer*>().size(), 0);
        QCOMPARE(service.findChildren<QTimer*>().size(), 1);
        auto* const timer = service.findChild<QTimer*>();
        QVERIFY(timer != nullptr);
        QCOMPARE(timer->interval(), 2000);
        QCOMPARE(timer->timerType(), Qt::CoarseTimer);

        QSignalSpy firstFrequencyChanged(
            &first, &Widgets::CpuViewModel::currentFrequencyMHzChanged);
        QSignalSpy secondHistoryChanged(&second, &Widgets::CpuViewModel::historyChanged);
        service.sampleNow();
        service.sampleNow();

        QCOMPARE(firstFrequencyChanged.count(), 2);
        QCOMPARE(secondHistoryChanged.count(), 1);
        QCOMPARE(first.physicalCores(), 8);
        QCOMPARE(second.logicalCores(), 16);
        QCOMPARE(first.maxFrequencyMHz(), 4000);
    }

    void invalidCpuSamplesPreserveLastGoodBaseline()
    {
        auto source = sourceWith({
            {Platform::CpuTimes{100, 200, 300}},
            std::nullopt,
            {Platform::CpuTimes{100, 200, 300}},
            {Platform::CpuTimes{90, 250, 350}},
            {Platform::CpuTimes{150, 300, 400}},
        });
        Platform::CpuService service(std::move(source));
        QSignalSpy historyChanged(&service, &Platform::CpuService::historyChanged);

        service.sampleNow();
        service.sampleNow();
        service.sampleNow();
        service.sampleNow();
        QCOMPARE(service.history().size(), 0);

        service.sampleNow();

        QCOMPARE(service.cpuPercent(), 75);
        QCOMPARE(service.history(), QVariantList{75});
        QCOMPARE(historyChanged.count(), 1);
    }

    void scalarSignalsOnlyOnChangeButHistorySignalsForEveryCalculatedSample()
    {
        auto source = sourceWith({
            {Platform::CpuTimes{0, 0, 0}},
            {Platform::CpuTimes{50, 100, 0}},
            {Platform::CpuTimes{100, 200, 0}},
        });
        Platform::CpuService service(std::move(source));
        QSignalSpy cpuChanged(&service, &Platform::CpuService::cpuPercentChanged);
        QSignalSpy historyChanged(&service, &Platform::CpuService::historyChanged);

        service.sampleNow();
        service.sampleNow();
        service.sampleNow();

        QCOMPARE(service.cpuPercent(), 50);
        QCOMPARE(cpuChanged.count(), 1);
        QCOMPARE(historyChanged.count(), 2);
        QCOMPARE(service.history(), QVariantList({50, 50}));
    }

    void cpuServiceStartAndStopAreIdempotent()
    {
        auto source = sourceWith({});
        Platform::CpuService service(std::move(source));
        auto* const timer = service.findChild<QTimer*>();
        QVERIFY(timer != nullptr);
        QVERIFY(!timer->isActive());

        service.start();
        service.start();
        QVERIFY(timer->isActive());

        service.stop();
        service.stop();
        QVERIFY(!timer->isActive());
    }

    void clockViewModelsHaveIndependentSettingsAndTimers()
    {
        const QDateTime now(
            QDate(2026, 9, 4), QTime(12, 34, 56, 789), QTimeZone("UTC"));
        Widgets::ClockViewModel first(
            Core::WidgetConfig{
                "clock-utc", "Clock", 0,
                QJsonObject{{"format", "HH:mm"}, {"timeZone", "UTC"}}},
            [now] { return now; });
        Widgets::ClockViewModel second(
            Core::WidgetConfig{
                "clock-shanghai", "Clock", 3,
                QJsonObject{{"format", "hh:mm AP"}, {"timeZone", "Asia/Shanghai"}}},
            [now] { return now; });

        QVERIFY(static_cast<QObject*>(&first) != static_cast<QObject*>(&second));
        QCOMPARE(first.instanceId(), QString("clock-utc"));
        QCOMPARE(second.instanceId(), QString("clock-shanghai"));
        QCOMPARE(first.timeText(), QString("12:34"));
        QCOMPARE(second.timeText(), QString("08:34 PM"));

        const auto firstTimers = first.findChildren<QTimer*>();
        const auto secondTimers = second.findChildren<QTimer*>();
        QCOMPARE(firstTimers.size(), 1);
        QCOMPARE(secondTimers.size(), 1);
        QVERIFY(firstTimers.front()->isSingleShot());
        QVERIFY(secondTimers.front()->isSingleShot());
        QCOMPARE(firstTimers.front()->timerType(), Qt::CoarseTimer);
        QCOMPARE(secondTimers.front()->timerType(), Qt::CoarseTimer);
        QVERIFY(firstTimers.front()->isActive());
        QVERIFY(secondTimers.front()->isActive());
    }

    void clockDeterministicUpdateUsesFallbacksWithoutWaiting()
    {
        QDateTime now(
            QDate(2026, 9, 4), QTime(12, 34, 0), QTimeZone("UTC"));
        Widgets::ClockViewModel clock(
            Core::WidgetConfig{
                "clock", "Clock", 0,
                QJsonObject{{"format", QJsonValue::Null}, {"timeZone", "invalid/zone"}}},
            [&now] { return now; });
        QCOMPARE(
            clock.timeText(),
            now.toTimeZone(QTimeZone::systemTimeZone()).toString("HH:mm"));

        QSignalSpy changed(&clock, &Widgets::ClockViewModel::timeTextChanged);
        now = now.addSecs(60);
        clock.Update();

        QCOMPARE(
            clock.timeText(),
            now.toTimeZone(QTimeZone::systemTimeZone()).toString("HH:mm"));
        QCOMPARE(changed.count(), 1);
    }
};

QTEST_GUILESS_MAIN(WidgetInstancesTest)
#include "widget_instances_test.moc"
