#include "platform/cpu_service.h"
#include "widgets/clock/clock_view_model.h"
#include "widgets/cpu/cpu_view_model.h"
#include "widgets/registry_setup.h"
#include "widgets/widget_descriptor.h"
#include "widgets/widget_registry.h"
#include "widgets/widget_view_model.h"

#include <QtTest>

#include <QJsonObject>
#include <QSignalSpy>

#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

static_assert(std::is_base_of_v<QObject, Widgets::WidgetViewModel>);
static_assert(!std::is_copy_constructible_v<Widgets::WidgetViewModel>);
static_assert(!std::is_copy_assignable_v<Widgets::WidgetViewModel>);

namespace {

class StubViewModel final : public Widgets::WidgetViewModel {
public:
    explicit StubViewModel(QString id)
        : m_id(std::move(id))
    {
    }

    [[nodiscard]] const QString& instanceId() const noexcept override { return m_id; }

private:
    QString m_id;
};

Widgets::WidgetDescriptor descriptor(
    QString type = QStringLiteral("Stub"),
    int span = 1,
    QUrl url = QUrl(QStringLiteral("qrc:/qt/qml/StatusBar/StubWidget.qml")))
{
    return {
        std::move(type),
        span,
        std::move(url),
        [](const Core::WidgetConfig& config, Widgets::WidgetContext&) {
            return std::make_unique<StubViewModel>(config.id);
        },
    };
}

class SequenceCpuDataSource final : public Platform::CpuDataSource {
public:
    std::optional<Platform::CpuTimes> sampleTimes() noexcept override
    {
        ++timesCalls;
        if (nextTimes >= times.size()) {
            return std::nullopt;
        }
        return times[nextTimes++];
    }

    std::optional<double> samplePerformanceRatio() noexcept override
    {
        return 0.5;
    }

    std::optional<Platform::CpuTopology> topology() noexcept override
    {
        return Platform::CpuTopology{4, 8, 4000};
    }

    std::vector<Platform::CpuTimes> times{
        {100, 200, 300},
        {150, 300, 300},
    };
    std::size_t nextTimes{};
    int timesCalls{};
};

Widgets::WidgetRegistry builtInRegistry()
{
    auto result = Widgets::registerAllWidgets();
    if (!result.hasValue()) {
        qFatal("Could not create built-in widget registry: %s", qPrintable(result.error()));
    }
    return std::move(result).value();
}

} // namespace

class WidgetRegistryTest final : public QObject {
    Q_OBJECT

private slots:
    void builtInDescriptorsHaveSafeInvariants()
    {
        const auto registry = builtInRegistry();
        const auto* const clock = registry.find(QStringLiteral("Clock"));
        const auto* const cpu = registry.find(QStringLiteral("Cpu"));

        QVERIFY(clock != nullptr);
        QVERIFY(cpu != nullptr);
        QVERIFY(clock != cpu);
        QVERIFY(!clock->type.isEmpty());
        QVERIFY(!cpu->type.isEmpty());
        QVERIFY(clock->type != cpu->type);
        QVERIFY(clock->span > 0);
        QVERIFY(cpu->span > 0);
        QVERIFY(clock->qmlUrl.isValid());
        QVERIFY(cpu->qmlUrl.isValid());
        QVERIFY(clock->qmlUrl.toString().startsWith("qrc:/qt/qml/StatusBar/"));
        QVERIFY(cpu->qmlUrl.toString().startsWith("qrc:/qt/qml/StatusBar/"));
        QVERIFY(static_cast<bool>(clock->create));
        QVERIFY(static_cast<bool>(cpu->create));
        QVERIFY(registry.find(QStringLiteral("Missing")) == nullptr);
    }

    void rejectsInvalidDescriptorLists()
    {
        const auto expectRejected = [](std::vector<Widgets::WidgetDescriptor> descriptors,
                                       QStringView expectedText) {
            const auto result = Widgets::WidgetRegistry::create(std::move(descriptors));
            QVERIFY(!result.hasValue());
            QVERIFY2(result.error().contains(expectedText, Qt::CaseInsensitive),
                     qPrintable(result.error()));
        };

        expectRejected(
            {descriptor(QStringLiteral("Duplicate")), descriptor(QStringLiteral("Duplicate"))},
            QStringView(u"duplicate"));
        expectRejected({descriptor(QString{})}, QStringView(u"type"));
        expectRejected({descriptor(QStringLiteral("ZeroSpan"), 0)}, QStringView(u"span"));
        expectRejected(
            {descriptor(QStringLiteral("EmptyUrl"), 1, QUrl{})}, QStringView(u"url"));
        expectRejected(
            {descriptor(QStringLiteral("RelativeUrl"), 1, QUrl("relative.qml"))},
            QStringView(u"url"));
        expectRejected(
            {descriptor(QStringLiteral("HttpUrl"), 1, QUrl("https://example.com/widget.qml"))},
            QStringView(u"qrc"));
        expectRejected(
            {descriptor(QStringLiteral("UnrootedQrcUrl"), 1, QUrl("qrc:widget.qml"))},
            QStringView(u"qrc"));

        auto emptyFactory = descriptor(QStringLiteral("EmptyFactory"));
        emptyFactory.create = {};
        expectRejected({std::move(emptyFactory)}, QStringView(u"factory"));
    }

    void independentRegistriesDoNotShareState()
    {
        auto firstResult = Widgets::WidgetRegistry::create(
            {descriptor(QStringLiteral("First"))});
        auto secondResult = Widgets::WidgetRegistry::create(
            {descriptor(QStringLiteral("Second"))});

        QVERIFY2(firstResult.hasValue(), qPrintable(firstResult.error()));
        QVERIFY2(secondResult.hasValue(), qPrintable(secondResult.error()));
        const auto first = std::move(firstResult).value();
        const auto second = std::move(secondResult).value();
        QVERIFY(first.find(QStringLiteral("First")) != nullptr);
        QVERIFY(first.find(QStringLiteral("Second")) == nullptr);
        QVERIFY(second.find(QStringLiteral("Second")) != nullptr);
        QVERIFY(second.find(QStringLiteral("First")) == nullptr);
    }

    void factoriesCreateDistinctTypedViewModelsUsingOneCpuService()
    {
        auto source = std::make_unique<SequenceCpuDataSource>();
        auto* const sourceCounts = source.get();
        Platform::CpuService cpuService(std::move(source));
        Widgets::WidgetContext context{cpuService};
        const auto registry = builtInRegistry();
        const auto* const clockDescriptor = registry.find(QStringLiteral("Clock"));
        const auto* const cpuDescriptor = registry.find(QStringLiteral("Cpu"));
        QVERIFY(clockDescriptor != nullptr);
        QVERIFY(cpuDescriptor != nullptr);

        auto firstClock = clockDescriptor->create(
            Core::WidgetConfig{"clock-one", "Clock", 0, {}}, context);
        auto secondClock = clockDescriptor->create(
            Core::WidgetConfig{"clock-two", "Clock", 3, {}}, context);
        auto firstCpu = cpuDescriptor->create(
            Core::WidgetConfig{"cpu-one", "Cpu", 6, {}}, context);
        QCOMPARE(sourceCounts->timesCalls, 1);
        auto secondCpu = cpuDescriptor->create(
            Core::WidgetConfig{"cpu-two", "Cpu", 8, {}}, context);
        QCOMPARE(sourceCounts->timesCalls, 1);

        QVERIFY(firstClock != nullptr);
        QVERIFY(secondClock != nullptr);
        QVERIFY(firstCpu != nullptr);
        QVERIFY(secondCpu != nullptr);
        QVERIFY(firstClock.get() != secondClock.get());
        QVERIFY(firstCpu.get() != secondCpu.get());
        QCOMPARE(firstClock->instanceId(), QString("clock-one"));
        QCOMPARE(secondClock->instanceId(), QString("clock-two"));
        QCOMPARE(firstCpu->instanceId(), QString("cpu-one"));
        QCOMPARE(secondCpu->instanceId(), QString("cpu-two"));
        QVERIFY(dynamic_cast<Widgets::ClockViewModel*>(firstClock.get()) != nullptr);
        auto* const firstCpuVm = dynamic_cast<Widgets::CpuViewModel*>(firstCpu.get());
        auto* const secondCpuVm = dynamic_cast<Widgets::CpuViewModel*>(secondCpu.get());
        QVERIFY(firstCpuVm != nullptr);
        QVERIFY(secondCpuVm != nullptr);
        QSignalSpy firstChanged(firstCpuVm, &Widgets::CpuViewModel::cpuPercentChanged);
        QSignalSpy secondChanged(secondCpuVm, &Widgets::CpuViewModel::cpuPercentChanged);

        cpuService.sampleNow();

        QCOMPARE(sourceCounts->timesCalls, 2);
        QCOMPARE(firstCpuVm->cpuPercent(), 50);
        QCOMPARE(secondCpuVm->cpuPercent(), 50);
        QCOMPARE(firstChanged.count(), 1);
        QCOMPARE(secondChanged.count(), 1);
    }
};

QTEST_GUILESS_MAIN(WidgetRegistryTest)
#include "widget_registry_test.moc"
