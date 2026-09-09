#include "widgets/registry_setup.h"

#include <QtTest>

#include <QFile>
#include <QStringList>

#include <utility>

class ResourceEmbeddingTest final : public QObject {
    Q_OBJECT

private slots:
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
};

QTEST_GUILESS_MAIN(ResourceEmbeddingTest)
#include "resource_embedding_test.moc"
