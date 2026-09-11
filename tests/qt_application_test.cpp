#include "core/config_repository.h"
#include "platform/cpu_data_source.h"
#include "ui/qt_application.h"
#include "widgets/cpu/cpu_view_model.h"
#include "widgets/registry_setup.h"
#include "widgets/widget_descriptor.h"
#include "widgets/widget_registry.h"

#include <QtTest>

#include <QFile>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTimer>

#include <array>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace {

struct SourceState final {
    int timesCalls{};
    int ratioCalls{};
    int topologyCalls{};
};

class FakeCpuDataSource final : public Platform::CpuDataSource {
public:
    explicit FakeCpuDataSource(std::shared_ptr<SourceState> state)
        : state_(std::move(state))
    {
    }

    std::optional<Platform::CpuTimes> sampleTimes() noexcept override
    {
        ++state_->timesCalls;
        return Platform::CpuTimes{};
    }

    std::optional<double> samplePerformanceRatio() noexcept override
    {
        ++state_->ratioCalls;
        return 1.0;
    }

    std::optional<Platform::CpuTopology> topology() noexcept override
    {
        ++state_->topologyCalls;
        return Platform::CpuTopology{4, 8, 3200};
    }

private:
    std::shared_ptr<SourceState> state_;
};

struct ApplicationArguments final {
    std::array<char, 20> executable{"qt_application_test"};
    int argc{1};
    std::array<char*, 2> argv{executable.data(), nullptr};
};

Core::Result<void> saveConfiguration(
    const Core::ConfigRepository& repository,
    QList<Core::WidgetConfig> widgets)
{
    return repository.save(Core::ConfigDocument{1, std::move(widgets)});
}

Core::Result<Widgets::WidgetRegistry> failingRegistry()
{
    std::vector<Widgets::WidgetDescriptor> descriptors;
    descriptors.push_back({
        QStringLiteral("Cpu"),
        2,
        QUrl(QStringLiteral("qrc:/qt/qml/StatusBar/CpuWidget.qml")),
        [](const Core::WidgetConfig& config, Widgets::WidgetContext& context)
            -> std::unique_ptr<Widgets::WidgetViewModel> {
            return std::make_unique<Widgets::CpuViewModel>(config, context.cpuService);
        },
    });
    descriptors.push_back({
        QStringLiteral("Broken"),
        1,
        QUrl(QStringLiteral("qrc:/qt/qml/StatusBar/ClockWidget.qml")),
        [](const Core::WidgetConfig&, Widgets::WidgetContext&)
            -> std::unique_ptr<Widgets::WidgetViewModel> {
            return nullptr;
        },
    });
    return Widgets::WidgetRegistry::create(std::move(descriptors));
}

} // namespace

class QtApplicationTest final : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        QTest::failOnWarning();
    }

    void initializesHiddenTypedRootWithoutContextProperties()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const Core::ConfigRepository repository(
            directory.filePath(QStringLiteral("config.json")));
        const auto saveResult = saveConfiguration(
            repository,
            {{QStringLiteral("clock"), QStringLiteral("Clock"), 0, {}}});
        QVERIFY2(saveResult.hasValue(), qPrintable(saveResult.error()));
        auto registryResult = Widgets::registerAllWidgets();
        QVERIFY2(registryResult.hasValue(), qPrintable(registryResult.error()));
        auto sourceState = std::make_shared<SourceState>();
        ApplicationArguments arguments;
        UI::QtApplication application(
            arguments.argc,
            arguments.argv.data(),
            repository,
            std::move(registryResult).value(),
            std::make_unique<FakeCpuDataSource>(sourceState));

        const auto initializeResult = application.initialize();

        QVERIFY2(initializeResult.hasValue(), qPrintable(initializeResult.error()));
        QQuickWindow* const window = application.window();
        QVERIFY(window != nullptr);
        QVERIFY(!window->isVisible());
        const QVariant rootModel = window->property("widgetModel");
        QVERIFY(rootModel.isValid());
        QVERIFY(!rootModel.isNull());
        QVERIFY(rootModel.value<UI::WidgetModel*>() != nullptr);
        QQmlContext* context = QQmlEngine::contextForObject(window);
        QVERIFY(context != nullptr);
        while (context->parentContext() != nullptr) {
            context = context->parentContext();
        }
        QVERIFY(!context->contextProperty(QStringLiteral("widgetModel")).isValid());
        QVERIFY(!context->contextProperty(QStringLiteral("clockAdapter")).isValid());
        QVERIFY(!context->contextProperty(QStringLiteral("cpuAdapter")).isValid());
        QCOMPARE(sourceState->topologyCalls, 1);
        QCOMPARE(sourceState->timesCalls, 0);
        QCOMPARE(sourceState->ratioCalls, 0);

        const auto secondInitializeResult = application.initialize();
        QVERIFY2(
            secondInitializeResult.hasValue(),
            qPrintable(secondInitializeResult.error()));
        QCOMPARE(application.window(), window);

        const auto showResult = application.show();
        QVERIFY2(showResult.hasValue(), qPrintable(showResult.error()));
        QTRY_VERIFY_WITH_TIMEOUT(window->isVisible(), 1000);
        QTimer::singleShot(0, [&application] { application.quit(); });
        QCOMPARE(application.run(), 0);
    }

    void reportsConfigurationFailureAndKeepsLifecycleSafe()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString configPath = directory.filePath(QStringLiteral("config.json"));
        QFile config(configPath);
        QVERIFY(config.open(QIODevice::WriteOnly));
        QCOMPARE(config.write("{ definitely not json"), qint64{21});
        config.close();
        auto registryResult = Widgets::registerAllWidgets();
        QVERIFY2(registryResult.hasValue(), qPrintable(registryResult.error()));
        auto sourceState = std::make_shared<SourceState>();
        ApplicationArguments arguments;
        UI::QtApplication application(
            arguments.argc,
            arguments.argv.data(),
            Core::ConfigRepository(configPath),
            std::move(registryResult).value(),
            std::make_unique<FakeCpuDataSource>(sourceState));

        application.quit();
        const auto initializeResult = application.initialize();

        QVERIFY(!initializeResult.hasValue());
        QVERIFY(initializeResult.error().contains(configPath));
        QVERIFY(initializeResult.error().contains(QStringLiteral("configuration")));
        QVERIFY(application.window() == nullptr);
        const auto showResult = application.show();
        QVERIFY(!showResult.hasValue());
        QVERIFY(showResult.error().contains(QStringLiteral("initialize")));
        application.quit();
        QCOMPARE(sourceState->timesCalls, 0);
    }

    void reportsInjectedQmlRootFailureWithoutChangingProductionModule()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const Core::ConfigRepository repository(
            directory.filePath(QStringLiteral("config.json")));
        const auto saveResult = saveConfiguration(
            repository,
            {{QStringLiteral("cpu"), QStringLiteral("Cpu"), 0, {}}});
        QVERIFY2(saveResult.hasValue(), qPrintable(saveResult.error()));
        auto registryResult = Widgets::registerAllWidgets();
        QVERIFY2(registryResult.hasValue(), qPrintable(registryResult.error()));
        auto sourceState = std::make_shared<SourceState>();
        ApplicationArguments arguments;
        UI::QtApplication application(
            arguments.argc,
            arguments.argv.data(),
            repository,
            std::move(registryResult).value(),
            std::make_unique<FakeCpuDataSource>(sourceState),
            {QStringLiteral("MissingStatusBarTestModule"), QStringLiteral("MissingRoot")});

        QTest::ignoreMessage(
            QtWarningMsg,
            QRegularExpression(QStringLiteral(
                "QQmlApplicationEngine failed to load component")));
        QTest::ignoreMessage(
            QtWarningMsg,
            QRegularExpression(QStringLiteral(
                ".*No module named \\\"MissingStatusBarTestModule\\\" found")));
        const auto initializeResult = application.initialize();

        QVERIFY(!initializeResult.hasValue());
        QVERIFY(initializeResult.error().contains(QStringLiteral("MissingStatusBarTestModule")));
        QVERIFY(initializeResult.error().contains(QStringLiteral("MissingRoot")));
        QVERIFY(application.window() == nullptr);
        const auto secondInitializeResult = application.initialize();
        QVERIFY(!secondInitializeResult.hasValue());
        QVERIFY(secondInitializeResult.error().contains(QStringLiteral("previous")));
        application.quit();
        QCOMPARE(sourceState->timesCalls, 1);
        QCOMPARE(sourceState->ratioCalls, 1);
        QTest::qWait(2100);
        QCOMPARE(sourceState->timesCalls, 1);
        QCOMPARE(sourceState->ratioCalls, 1);
    }

    void failedModelStagingReleasesCpuConsumerLease()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const Core::ConfigRepository repository(
            directory.filePath(QStringLiteral("config.json")));
        const auto saveResult = saveConfiguration(
            repository,
            {
                {QStringLiteral("cpu"), QStringLiteral("Cpu"), 0, {}},
                {QStringLiteral("broken"), QStringLiteral("Broken"), 2, {}},
            });
        QVERIFY2(saveResult.hasValue(), qPrintable(saveResult.error()));
        auto registryResult = failingRegistry();
        QVERIFY2(registryResult.hasValue(), qPrintable(registryResult.error()));
        auto sourceState = std::make_shared<SourceState>();
        ApplicationArguments arguments;
        UI::QtApplication application(
            arguments.argc,
            arguments.argv.data(),
            repository,
            std::move(registryResult).value(),
            std::make_unique<FakeCpuDataSource>(sourceState));

        const auto initializeResult = application.initialize();

        QVERIFY(!initializeResult.hasValue());
        QVERIFY(initializeResult.error().contains(QStringLiteral("broken")));
        QCOMPARE(sourceState->timesCalls, 1);
        QCOMPARE(sourceState->ratioCalls, 1);
        QTest::qWait(2100);
        QCOMPARE(sourceState->timesCalls, 1);
        QCOMPARE(sourceState->ratioCalls, 1);
    }
};

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QtApplicationTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "qt_application_test.moc"
