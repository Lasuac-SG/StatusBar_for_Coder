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

bool fireSingleShotTimer(QTimer* timer)
{
    timer->stop();
    return QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection);
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

    void cpuViewModelCachesValuesAfterServiceDestruction()
    {
        auto source = sourceWith(
            {{Platform::CpuTimes{0, 0, 0}}, {Platform::CpuTimes{50, 100, 0}}},
            {{1.0}, {0.5}});
        auto service = std::make_unique<Platform::CpuService>(std::move(source));
        service->sampleNow();

        auto viewModel = std::make_unique<Widgets::CpuViewModel>(
            Core::WidgetConfig{"cpu", "Cpu", 0, QJsonObject{{"label", "safe"}}},
            *service);
        QSignalSpy cpuChanged(viewModel.get(), &Widgets::CpuViewModel::cpuPercentChanged);
        QSignalSpy frequencyChanged(
            viewModel.get(), &Widgets::CpuViewModel::currentFrequencyMHzChanged);
        QSignalSpy historyChanged(viewModel.get(), &Widgets::CpuViewModel::historyChanged);

        service->sampleNow();

        QCOMPARE(cpuChanged.count(), 1);
        QCOMPARE(frequencyChanged.count(), 1);
        QCOMPARE(historyChanged.count(), 1);
        const QVariantList expectedHistory{50};
        QCOMPARE(viewModel->cpuPercent(), 50);
        QCOMPARE(viewModel->currentFrequencyMHz(), 2000);
        QCOMPARE(viewModel->history(), expectedHistory);

        service.reset();

        QCOMPARE(viewModel->cpuPercent(), 50);
        QCOMPARE(viewModel->currentFrequencyMHz(), 2000);
        QCOMPARE(viewModel->maxFrequencyMHz(), 4000);
        QCOMPARE(viewModel->physicalCores(), 8);
        QCOMPARE(viewModel->logicalCores(), 16);
        QCOMPARE(viewModel->history(), expectedHistory);
        QCOMPARE(viewModel->property("cpuPercent").toInt(), 50);
        QCOMPARE(viewModel->property("currentFrequencyMHz").toInt(), 2000);
        QCOMPARE(viewModel->property("maxFrequencyMHz").toInt(), 4000);
        QCOMPARE(viewModel->property("physicalCores").toInt(), 8);
        QCOMPARE(viewModel->property("logicalCores").toInt(), 16);
        QCOMPARE(viewModel->property("history").toList(), expectedHistory);
        QCOMPARE(viewModel->property("instanceId").toString(), QString("cpu"));
        const QJsonObject expectedSettings{{"label", "safe"}};
        QCOMPARE(viewModel->property("settings").toJsonObject(), expectedSettings);
        QCOMPARE(cpuChanged.count(), 1);
        QCOMPARE(frequencyChanged.count(), 1);
        QCOMPARE(historyChanged.count(), 1);
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
        auto* const counts = source.get();
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
        QCOMPARE(counts->timesCalls, 5);
        QCOMPARE(counts->ratioCalls, 5);
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

    void cpuHistoryRollsOverInOldestToNewestOrder()
    {
        auto source = std::make_unique<CountingCpuDataSource>();
        Platform::CpuTimes current{};
        source->times.push_back(current);
        for (int usage = 0; usage < 32; ++usage) {
            current.idle += static_cast<std::uint64_t>(100 - usage);
            current.kernel += 100;
            source->times.push_back(current);
        }
        auto* const counts = source.get();
        Platform::CpuService service(std::move(source));

        for (int sample = 0; sample < 33; ++sample) {
            service.sampleNow();
        }

        QVariantList expected;
        for (int usage = 2; usage < 32; ++usage) {
            expected.append(usage);
        }
        QCOMPARE(counts->timesCalls, 33);
        QCOMPARE(service.history().size(), 30);
        QCOMPARE(service.history(), expected);
        const QVariantList* const cachedHistory = &service.history();
        QCOMPARE(&service.history(), cachedHistory);
    }

    void failedTopologyUsesZeroFallbacks()
    {
        auto source = sourceWith({});
        auto* const counts = source.get();
        source->cpuTopology = std::nullopt;
        Platform::CpuService service(std::move(source));
        Widgets::CpuViewModel viewModel(
            Core::WidgetConfig{"cpu", "Cpu", 0, {}}, service);

        QCOMPARE(counts->topologyCalls, 1);
        QCOMPARE(service.maxFrequencyMHz(), 0);
        QCOMPARE(service.physicalCores(), 0);
        QCOMPARE(service.logicalCores(), 0);
        QCOMPARE(viewModel.maxFrequencyMHz(), 0);
        QCOMPARE(viewModel.physicalCores(), 0);
        QCOMPARE(viewModel.logicalCores(), 0);
    }

    void cpuServiceStartPrimesOnceAndIsIdempotent()
    {
        auto source = sourceWith(
            {{Platform::CpuTimes{0, 0, 0}}, {Platform::CpuTimes{50, 100, 0}}},
            {{1.0}, {0.5}});
        auto* const counts = source.get();
        Platform::CpuService service(std::move(source));
        auto* const timer = service.findChild<QTimer*>();
        QVERIFY(timer != nullptr);
        QVERIFY(!timer->isActive());

        service.start();
        QCOMPARE(counts->timesCalls, 1);
        QCOMPARE(counts->ratioCalls, 1);
        QVERIFY(timer->isActive());

        service.start();
        QCOMPARE(counts->timesCalls, 1);
        QCOMPARE(counts->ratioCalls, 1);
        QVERIFY(timer->isActive());

        service.sampleNow();
        QCOMPARE(service.cpuPercent(), 50);

        service.stop();
        service.stop();
        QVERIFY(!timer->isActive());
    }

    void cpuServiceWithNullSourceNeverStarts()
    {
        Platform::CpuService service(std::unique_ptr<Platform::CpuDataSource>{});
        auto* const timer = service.findChild<QTimer*>();
        QVERIFY(timer != nullptr);

        service.start();
        service.start();

        QVERIFY(!timer->isActive());
        QCOMPARE(service.cpuPercent(), 0);
        QCOMPARE(service.currentFrequencyMHz(), 0);
        QCOMPARE(service.history(), QVariantList{});
    }

    void cpuServiceRestartPrimesAFreshBaseline()
    {
        auto source = sourceWith(
            {{Platform::CpuTimes{0, 0, 0}},
             {Platform::CpuTimes{50, 100, 0}},
             {Platform::CpuTimes{1000, 2000, 0}},
             {Platform::CpuTimes{1010, 2100, 0}}},
            {{1.0}, {0.5}, {0.75}, {0.8}});
        auto* const counts = source.get();
        Platform::CpuService service(std::move(source));

        service.start();
        service.sampleNow();
        QCOMPARE(service.cpuPercent(), 50);
        QCOMPARE(service.history(), QVariantList{50});

        service.stop();
        service.start();
        service.start();

        QCOMPARE(counts->timesCalls, 3);
        QCOMPARE(counts->ratioCalls, 3);
        QCOMPARE(service.cpuPercent(), 50);
        QCOMPARE(service.history(), QVariantList{50});

        service.sampleNow();

        QCOMPARE(counts->timesCalls, 4);
        QCOMPARE(counts->ratioCalls, 4);
        QCOMPARE(service.cpuPercent(), 90);
        QCOMPARE(service.history(), QVariantList({50, 90}));
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

    void clockEarlyTimeoutWaitsForStoredMinuteTarget()
    {
        QDateTime now(
            QDate(2026, 9, 4), QTime(12, 34, 56, 789), QTimeZone("UTC"));
        Widgets::ClockViewModel clock(
            Core::WidgetConfig{
                "clock", "Clock", 0,
                QJsonObject{{"format", "HH:mm:ss.zzz"}, {"timeZone", "UTC"}}},
            [&now] { return now; });
        auto* const timer = clock.findChild<QTimer*>();
        QVERIFY(timer != nullptr);
        QCOMPARE(clock.timeText(), QString("12:34:56.789"));
        QCOMPARE(timer->timerType(), Qt::CoarseTimer);
        QCOMPARE(timer->interval(), 3211);
        QSignalSpy changed(&clock, &Widgets::ClockViewModel::timeTextChanged);

        now = QDateTime(
            QDate(2026, 9, 4), QTime(12, 34, 59, 900), QTimeZone("UTC"));
        QVERIFY(fireSingleShotTimer(timer));

        QCOMPARE(clock.timeText(), QString("12:34:56.789"));
        QCOMPARE(changed.count(), 0);
        QVERIFY(timer->isActive());
        QVERIFY(timer->isSingleShot());
        QCOMPARE(timer->timerType(), Qt::PreciseTimer);
        QCOMPARE(timer->interval(), 100);

        now = QDateTime(
            QDate(2026, 9, 4), QTime(12, 35, 0, 0), QTimeZone("UTC"));
        QVERIFY(fireSingleShotTimer(timer));

        QCOMPARE(clock.timeText(), QString("12:35:00.000"));
        QCOMPARE(changed.count(), 1);
        QVERIFY(timer->isActive());
        QCOMPARE(timer->timerType(), Qt::CoarseTimer);
        QCOMPARE(timer->interval(), 60000);
    }

    void clockUsesPreciseTimerForShortMillisecondRemainder()
    {
        const QDateTime now(
            QDate(2026, 9, 4), QTime(12, 34, 59, 875), QTimeZone("UTC"));
        Widgets::ClockViewModel clock(
            Core::WidgetConfig{
                "clock", "Clock", 0,
                QJsonObject{{"format", "HH:mm"}, {"timeZone", "UTC"}}},
            [now] { return now; });
        auto* const timer = clock.findChild<QTimer*>();
        QVERIFY(timer != nullptr);

        QVERIFY(timer->isActive());
        QVERIFY(timer->isSingleShot());
        QCOMPARE(timer->timerType(), Qt::PreciseTimer);
        QCOMPARE(timer->interval(), 125);
    }
};

QTEST_GUILESS_MAIN(WidgetInstancesTest)
#include "widget_instances_test.moc"
