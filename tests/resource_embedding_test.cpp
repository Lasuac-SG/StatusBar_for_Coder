#include "platform/cpu_service.h"
#include "ui/widget_model.h"
#include "widgets/clock/clock_view_model.h"
#include "widgets/cpu/cpu_view_model.h"
#include "widgets/registry_setup.h"
#include "widgets/widget_view_model.h"

#include <QtTest>

#include <QFile>
#include <QQuickWindow>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QStringList>
#include <QtQml>

#include <memory>
#include <optional>
#include <utility>

namespace {

class NullCpuDataSource final : public Platform::CpuDataSource {
public:
    std::optional<Platform::CpuTimes> sampleTimes() noexcept override
    {
        return std::nullopt;
    }
    std::optional<double> samplePerformanceRatio() noexcept override
    {
        return std::nullopt;
    }
    std::optional<Platform::CpuTopology> topology() noexcept override
    {
        return std::nullopt;
    }
};

QString errorText(const QQmlComponent& component)
{
    QStringList messages;
    for (const auto& error : component.errors()) {
        messages.append(error.toString());
    }
    return messages.join(QLatin1Char('\n'));
}

void registerQmlTypesForTest()
{
    qmlRegisterUncreatableType<UI::WidgetModel>(
        "StatusBar", 1, 0, "WidgetModel", "WidgetModel is created by C++");
    qmlRegisterUncreatableType<Widgets::WidgetViewModel>(
        "StatusBar", 1, 0, "WidgetViewModel", "WidgetViewModel is created by C++");
    qmlRegisterUncreatableType<Widgets::ClockViewModel>(
        "StatusBar", 1, 0, "ClockViewModel", "ClockViewModel is created by C++");
    qmlRegisterUncreatableType<Widgets::CpuViewModel>(
        "StatusBar", 1, 0, "CpuViewModel", "CpuViewModel is created by C++");
}

struct ModelFixture final {
    QTemporaryDir directory;
    Platform::CpuService cpuService{std::make_unique<NullCpuDataSource>()};
    Widgets::WidgetContext context{cpuService};
    UI::WidgetModel model;

    explicit ModelFixture(QList<Core::WidgetConfig> widgets)
        : model(
              Core::ConfigRepository(directory.filePath(QStringLiteral("config.json"))),
              registry(),
              context)
    {
        Q_ASSERT(directory.isValid());
        Core::ConfigRepository repository(directory.filePath(QStringLiteral("config.json")));
        Q_ASSERT(repository.save(Core::ConfigDocument{1, std::move(widgets)}).hasValue());
        Q_ASSERT(model.loadFromConfig().hasValue());
    }

private:
    static Widgets::WidgetRegistry registry()
    {
        auto result = Widgets::registerAllWidgets();
        Q_ASSERT(result.hasValue());
        return std::move(result).value();
    }
};

} // namespace

class ResourceEmbeddingTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        registerQmlTypesForTest();
    }

    void exposesOnlyModuleResourcesAtStableUrls()
    {
        const QStringList resourcePaths{
            QStringLiteral(":/qt/qml/StatusBar/Main.qml"),
            QStringLiteral(":/qt/qml/StatusBar/Theme.qml"),
            QStringLiteral(":/qt/qml/StatusBar/WidgetHost.qml"),
            QStringLiteral(":/qt/qml/StatusBar/MetricCard.qml"),
            QStringLiteral(":/qt/qml/StatusBar/ClockWidget.qml"),
            QStringLiteral(":/qt/qml/StatusBar/CpuWidget.qml"),
            QStringLiteral(":/qt/qml/StatusBar/CpuDetailPopup.qml"),
            QStringLiteral(":/qt/qml/StatusBar/cpu.svg"),
        };

        for (const auto& path : resourcePaths) {
            QFile resource(path);
            QVERIFY2(resource.open(QIODevice::ReadOnly), qPrintable(path));
            QVERIFY2(!resource.readAll().isEmpty(), qPrintable(path));
        }
    }

    void descriptorUrlsResolveToModuleComponents()
    {
        auto registryResult = Widgets::registerAllWidgets();
        QVERIFY2(registryResult.hasValue(), qPrintable(registryResult.error()));
        const auto registry = std::move(registryResult).value();
        QCOMPARE(
            registry.find(QStringLiteral("Clock"))->qmlUrl,
            QUrl(QStringLiteral("qrc:/qt/qml/StatusBar/ClockWidget.qml")));
        QCOMPARE(
            registry.find(QStringLiteral("Cpu"))->qmlUrl,
            QUrl(QStringLiteral("qrc:/qt/qml/StatusBar/CpuWidget.qml")));

        for (const auto& descriptor : registry.descriptors()) {
            QFile resource(QStringLiteral(":") + descriptor.qmlUrl.path());
            QVERIFY2(resource.open(QIODevice::ReadOnly), qPrintable(descriptor.qmlUrl.toString()));
        }
    }

    void mainRequiresARealWidgetModelAndStartsHidden()
    {
        ModelFixture fixture({});
        QQmlEngine engine;
        engine.addImportPath(QString::fromUtf8(STATUSBAR_QML_IMPORT_PATH));
        QVERIFY(!engine.rootContext()->contextProperty(QStringLiteral("widgetModel")).isValid());
        QVERIFY(!engine.rootContext()->contextProperty(QStringLiteral("clockAdapter")).isValid());
        QVERIFY(!engine.rootContext()->contextProperty(QStringLiteral("cpuAdapter")).isValid());

        QQmlComponent component(
            &engine, QUrl(QStringLiteral("qrc:/qt/qml/StatusBar/Main.qml")));
        QVERIFY2(component.status() == QQmlComponent::Ready, qPrintable(errorText(component)));
        std::unique_ptr<QObject> root(component.createWithInitialProperties({
            {QStringLiteral("widgetModel"), QVariant::fromValue(&fixture.model)},
        }));

        QVERIFY2(root != nullptr, qPrintable(errorText(component)));
        auto* const window = qobject_cast<QQuickWindow*>(root.get());
        QVERIFY(window != nullptr);
        QVERIFY(!window->isVisible());
        QCOMPARE(root->property("widgetModel").value<UI::WidgetModel*>(), &fixture.model);
    }

    void descriptorComponentsUseTypedViewModelsAndCpuDetailIsLazy()
    {
        ModelFixture fixture({
            {"clock", "Clock", 0, {}},
            {"cpu", "Cpu", 3, {}},
        });
        QQmlEngine engine;
        engine.addImportPath(QString::fromUtf8(STATUSBAR_QML_IMPORT_PATH));
        QQuickWindow editingWindow;

        for (int row = 0; row < fixture.model.rowCount(); ++row) {
            const auto index = fixture.model.index(row);
            const QUrl qmlUrl = fixture.model.data(index, UI::WidgetModel::QmlUrlRole).toUrl();
            auto* const viewModel = fixture.model
                                        .data(index, UI::WidgetModel::ViewModelRole)
                                        .value<Widgets::WidgetViewModel*>();
            QVERIFY(viewModel != nullptr);
            QQmlComponent component(&engine, qmlUrl);
            QVERIFY2(component.status() == QQmlComponent::Ready, qPrintable(errorText(component)));
            std::unique_ptr<QObject> object(component.createWithInitialProperties({
                {QStringLiteral("viewModel"), QVariant::fromValue(viewModel)},
                {QStringLiteral("editingWindow"),
                 QVariant::fromValue(static_cast<QObject*>(&editingWindow))},
                {QStringLiteral("editing"), false},
            }));
            QVERIFY2(object != nullptr, qPrintable(errorText(component)));
            QCOMPARE(
                object->property("viewModel").value<Widgets::WidgetViewModel*>(),
                viewModel);

            if (qmlUrl.fileName() == QStringLiteral("CpuWidget.qml")) {
                QObject* const detailLoader = object->findChild<QObject*>(
                    QStringLiteral("cpuDetailLoader"));
                QVERIFY(detailLoader != nullptr);
                QVERIFY(!detailLoader->property("active").toBool());
                QVERIFY(object->findChild<QObject*>(QStringLiteral("cpuDetailPopup")) == nullptr);
            }
        }
    }
};

QTEST_MAIN(ResourceEmbeddingTest)
#include "resource_embedding_test.moc"
