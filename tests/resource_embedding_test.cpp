#include "platform/cpu_service.h"
#include "widgets/registry_setup.h"

#include <QtTest>

#include <QFile>
#include <QQmlComponent>
#include <QQmlEngine>
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

} // namespace

class ResourceEmbeddingTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(qmlRegisterSingletonType(
                    QUrl(QStringLiteral("qrc:/src/ui/qml/Theme.qml")),
                    "Theme",
                    1,
                    0,
                    "Theme")
                >= 0);
    }

    void exposesTransitionalQmlResources()
    {
        const QStringList resourcePaths{
            QStringLiteral(":/src/ui/qml/main.qml"),
            QStringLiteral(":/src/ui/qml/Theme.qml"),
            QStringLiteral(":/src/ui/qml/qmldir"),
            QStringLiteral(":/qt/qml/StatusBar/ClockWidget.qml"),
            QStringLiteral(":/qt/qml/StatusBar/CpuWidget.qml"),
            QStringLiteral(":/qt/qml/StatusBar/CpuDetailPopup.qml"),
            QStringLiteral(":/src/widgets/cpu/cpu.svg"),
        };

        for (const auto& path : resourcePaths) {
            QFile resource(path);
            QVERIFY2(resource.open(QIODevice::ReadOnly), qPrintable(path));
            QVERIFY2(!resource.readAll().isEmpty(), qPrintable(path));
        }
    }

    void descriptorUrlsResolveToEmbeddedAliases()
    {
        auto registryResult = Widgets::registerAllWidgets();
        QVERIFY2(registryResult.hasValue(), qPrintable(registryResult.error()));
        const auto registry = std::move(registryResult).value();

        for (const auto& descriptor : registry.descriptors()) {
            const QString resourcePath = QStringLiteral(":") + descriptor.qmlUrl.path();
            QFile resource(resourcePath);
            QVERIFY2(resource.open(QIODevice::ReadOnly), qPrintable(resourcePath));
            QVERIFY2(!resource.readAll().isEmpty(), qPrintable(resourcePath));
        }
    }

    void descriptorComponentsInstantiateWithRealViewModels()
    {
        auto registryResult = Widgets::registerAllWidgets();
        QVERIFY2(registryResult.hasValue(), qPrintable(registryResult.error()));
        const auto registry = std::move(registryResult).value();
        Platform::CpuService cpuService(std::make_unique<NullCpuDataSource>());
        Widgets::WidgetContext context{cpuService};
        QQmlEngine engine;
        const QList<Core::WidgetConfig> configs{
            {"clock", "Clock", 0, {}},
            {"cpu", "Cpu", 3, {}},
        };

        for (const auto& config : configs) {
            const auto* const descriptor = registry.find(config.type);
            QVERIFY(descriptor != nullptr);
            auto viewModel = descriptor->create(config, context);
            QVERIFY(viewModel != nullptr);
            QQmlEngine::setObjectOwnership(viewModel.get(), QQmlEngine::CppOwnership);

            QQmlComponent component(&engine, descriptor->qmlUrl);
            QVERIFY2(component.status() == QQmlComponent::Ready,
                     qPrintable(errorText(component)));
            std::unique_ptr<QObject> object(component.createWithInitialProperties({
                {QStringLiteral("viewModel"),
                 QVariant::fromValue(static_cast<QObject*>(viewModel.get()))},
            }));
            QVERIFY2(object != nullptr, qPrintable(errorText(component)));
            QCOMPARE(object->property("viewModel").value<QObject*>(), viewModel.get());
        }
    }
};

QTEST_MAIN(ResourceEmbeddingTest)
#include "resource_embedding_test.moc"
