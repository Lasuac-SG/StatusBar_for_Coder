#include "platform/cpu_service.h"
#include "ui/window_geometry.h"
#include "ui/widget_model.h"
#include "widgets/clock/clock_view_model.h"
#include "widgets/cpu/cpu_view_model.h"
#include "widgets/registry_setup.h"
#include "widgets/widget_view_model.h"

#include <QtTest>

#include <QAccessible>
#include <QFile>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QStringList>

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

std::unique_ptr<QObject> createWidgetObject(
    QQmlComponent& component,
    Widgets::WidgetViewModel* viewModel,
    QQuickWindow* editingWindow)
{
    return std::unique_ptr<QObject>(component.createWithInitialProperties({
        {QStringLiteral("viewModel"), QVariant::fromValue(viewModel)},
        {QStringLiteral("editingWindow"), QVariant::fromValue(editingWindow)},
        {QStringLiteral("editing"), false},
    }));
}

QQuickItem* findQuickItem(QQuickItem* root, const QString& objectName)
{
    if (root->objectName() == objectName) {
        return root;
    }
    for (QQuickItem* const child : root->childItems()) {
        if (QQuickItem* const match = findQuickItem(child, objectName)) {
            return match;
        }
    }
    return nullptr;
}

QObject* findQuickObjectByClassName(QQuickItem* root, QByteArrayView classNameFragment)
{
    for (QObject* const child : root->children()) {
        if (QByteArrayView(child->metaObject()->className()).contains(classNameFragment)) {
            return child;
        }
    }
    for (QQuickItem* const child : root->childItems()) {
        if (QObject* const match = findQuickObjectByClassName(child, classNameFragment)) {
            return match;
        }
    }
    return nullptr;
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
    void init()
    {
        QTest::failOnWarning();
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
        QQmlApplicationEngine engine;
        QSignalSpy qmlWarnings(&engine, &QQmlEngine::warnings);
        QVERIFY(!engine.rootContext()->contextProperty(QStringLiteral("widgetModel")).isValid());
        QVERIFY(!engine.rootContext()->contextProperty(QStringLiteral("clockAdapter")).isValid());
        QVERIFY(!engine.rootContext()->contextProperty(QStringLiteral("cpuAdapter")).isValid());

        engine.setInitialProperties({
            {QStringLiteral("widgetModel"), QVariant::fromValue(&fixture.model)},
        });
        engine.loadFromModule(QStringLiteral("StatusBar"), QStringLiteral("Main"));

        QCOMPARE(engine.rootObjects().size(), 1);
        auto* const root = engine.rootObjects().constFirst();
        auto* const window = qobject_cast<QQuickWindow*>(root);
        QVERIFY(window != nullptr);
        QVERIFY(!window->isVisible());
        QCOMPARE(root->property("widgetModel").value<UI::WidgetModel*>(), &fixture.model);
        QCOMPARE(qmlWarnings.count(), 0);
    }

    void descriptorComponentsUseTypedViewModelsAndCpuDetailIsLazy()
    {
        ModelFixture fixture({
            {"clock", "Clock", 0, {}},
            {"cpu", "Cpu", 3, {}},
        });
        QQmlEngine engine;
        QSignalSpy qmlWarnings(&engine, &QQmlEngine::warnings);
        QQmlComponent editingWindowComponent(&engine);
        editingWindowComponent.setData(
            QByteArrayLiteral("import QtQuick.Window\nWindow { visible: false }"),
            QUrl());
        QVERIFY2(
            editingWindowComponent.status() == QQmlComponent::Ready,
            qPrintable(errorText(editingWindowComponent)));
        std::unique_ptr<QQuickWindow> editingWindow(
            qobject_cast<QQuickWindow*>(editingWindowComponent.create()));
        QVERIFY2(editingWindow != nullptr, qPrintable(errorText(editingWindowComponent)));

        for (int row = 0; row < fixture.model.rowCount(); ++row) {
            const auto index = fixture.model.index(row);
            const QUrl qmlUrl = fixture.model.data(index, UI::WidgetModel::QmlUrlRole).toUrl();
            auto* const viewModel = fixture.model
                                        .data(index, UI::WidgetModel::ViewModelRole)
                                        .value<Widgets::WidgetViewModel*>();
            QVERIFY(viewModel != nullptr);
            QQmlComponent component(&engine, qmlUrl);
            QVERIFY2(component.status() == QQmlComponent::Ready, qPrintable(errorText(component)));
            auto object = createWidgetObject(component, viewModel, editingWindow.get());
            QVERIFY2(object != nullptr, qPrintable(errorText(component)));
            QCOMPARE(qmlWarnings.count(), 0);
            QCOMPARE(
                object->property("viewModel").value<Widgets::WidgetViewModel*>(),
                viewModel);
            QCOMPARE(
                object->property("editingWindow").value<QQuickWindow*>(),
                editingWindow.get());

            if (qmlUrl.fileName() == QStringLiteral("CpuWidget.qml")) {
                QObject* const detailLoader = object->findChild<QObject*>(
                    QStringLiteral("cpuDetailLoader"));
                QVERIFY(detailLoader != nullptr);
                QVERIFY(!detailLoader->property("active").toBool());
                QVERIFY(object->findChild<QObject*>(QStringLiteral("cpuDetailPopup")) == nullptr);

                QVERIFY(detailLoader->setProperty("active", true));
                QObject* const detailPopup = detailLoader->property("item").value<QObject*>();
                QVERIFY(detailPopup != nullptr);
                QCOMPARE(
                    detailPopup->property("viewModel").value<Widgets::WidgetViewModel*>(),
                    viewModel);
                QCOMPARE(
                    detailPopup->property("editingWindow").value<QQuickWindow*>(),
                    editingWindow.get());
                QCOMPARE(qmlWarnings.count(), 0);
            }
        }
    }

    void widgetHostPreservesTypedInitialProperties()
    {
        ModelFixture fixture({
            {"clock", "Clock", 0, {}},
            {"cpu", "Cpu", 3, {}},
        });
        QQmlEngine engine;
        QSignalSpy qmlWarnings(&engine, &QQmlEngine::warnings);
        QQmlComponent editingWindowComponent(&engine);
        editingWindowComponent.setData(
            QByteArrayLiteral("import QtQuick.Window\nWindow { visible: false }"), QUrl());
        QVERIFY2(
            editingWindowComponent.status() == QQmlComponent::Ready,
            qPrintable(errorText(editingWindowComponent)));
        std::unique_ptr<QQuickWindow> editingWindow(
            qobject_cast<QQuickWindow*>(editingWindowComponent.create()));
        QVERIFY2(editingWindow != nullptr, qPrintable(errorText(editingWindowComponent)));

        QQmlComponent hostComponent(
            &engine, QUrl(QStringLiteral("qrc:/qt/qml/StatusBar/WidgetHost.qml")));
        QVERIFY2(
            hostComponent.status() == QQmlComponent::Ready,
            qPrintable(errorText(hostComponent)));

        for (int row = 0; row < fixture.model.rowCount(); ++row) {
            const auto index = fixture.model.index(row);
            const QUrl qmlUrl = fixture.model.data(index, UI::WidgetModel::QmlUrlRole).toUrl();
            auto* const viewModel = fixture.model
                                        .data(index, UI::WidgetModel::ViewModelRole)
                                        .value<Widgets::WidgetViewModel*>();
            QVERIFY(viewModel != nullptr);
            std::unique_ptr<QObject> host(hostComponent.createWithInitialProperties({
                {QStringLiteral("qmlUrl"), QVariant::fromValue(qmlUrl)},
                {QStringLiteral("viewModel"), QVariant::fromValue(viewModel)},
                {QStringLiteral("editingWindow"), QVariant::fromValue(editingWindow.get())},
                {QStringLiteral("editing"), false},
            }));
            QVERIFY2(host != nullptr, qPrintable(errorText(hostComponent)));

            QObject* const loader = host->findChild<QObject*>(QStringLiteral("widgetLoader"));
            QVERIFY(loader != nullptr);
            QTRY_VERIFY_WITH_TIMEOUT(loader->property("item").value<QObject*>() != nullptr, 1000);
            QObject* const widget = loader->property("item").value<QObject*>();
            QVERIFY(widget != nullptr);
            QCOMPARE(
                widget->property("viewModel").value<Widgets::WidgetViewModel*>(),
                viewModel);
            QCOMPARE(
                widget->property("editingWindow").value<QQuickWindow*>(),
                editingWindow.get());
            QCOMPARE(qmlWarnings.count(), 0);
        }
    }

    void cpuLongPressEntersEditingOnceAndSuppressesClick()
    {
        ModelFixture fixture({
            {"cpu", "Cpu", 0, {}},
        });
        QQmlApplicationEngine engine;
        QSignalSpy qmlWarnings(&engine, &QQmlEngine::warnings);
        engine.setInitialProperties({
            {QStringLiteral("widgetModel"), QVariant::fromValue(&fixture.model)},
        });
        engine.loadFromModule(QStringLiteral("StatusBar"), QStringLiteral("Main"));

        QCOMPARE(engine.rootObjects().size(), 1);
        QObject* const root = engine.rootObjects().constFirst();
        auto* const window = qobject_cast<QQuickWindow*>(root);
        QVERIFY(window != nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(
            findQuickItem(window->contentItem(), QStringLiteral("widgetLoader")) != nullptr,
            1000);
        QObject* const loader = findQuickItem(
            window->contentItem(), QStringLiteral("widgetLoader"));
        QVERIFY(loader != nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(loader->property("item").value<QObject*>() != nullptr, 1000);
        auto* const cpuWidget = qobject_cast<QQuickItem*>(
            loader->property("item").value<QObject*>());
        QVERIFY(cpuWidget != nullptr);
        QObject* const detailLoader = cpuWidget->findChild<QObject*>(
            QStringLiteral("cpuDetailLoader"));
        QVERIFY(detailLoader != nullptr);
        QSignalSpy editingChanged(root, SIGNAL(isEditingChanged()));
        QVERIFY(editingChanged.isValid());

        QVERIFY(QMetaObject::invokeMethod(cpuWidget, "handleLongPress"));
        QCOMPARE(root->property("isEditing").toBool(), true);
        QCOMPARE(editingChanged.count(), 1);

        QObject* const editTapHandler = findQuickObjectByClassName(
            window->contentItem(), QByteArrayView("TapHandler"));
        QVERIFY(editTapHandler != nullptr);
        QVERIFY(QMetaObject::invokeMethod(editTapHandler, "longPressed"));
        QCOMPARE(root->property("isEditing").toBool(), true);
        QCOMPARE(editingChanged.count(), 1);

        QVERIFY(QMetaObject::invokeMethod(cpuWidget, "handleClick"));
        QCOMPARE(root->property("isEditing").toBool(), true);
        QCOMPARE(detailLoader->property("active").toBool(), false);

        QFile config(fixture.directory.filePath(QStringLiteral("config.json")));
        QVERIFY(config.open(QIODevice::ReadOnly));
        const QByteArray configBeforeClick = config.readAll();
        config.close();

        window->show();
        QTRY_VERIFY_WITH_TIMEOUT(window->isVisible(), 1000);
        const QPoint clickPosition = cpuWidget
                                         ->mapToScene(QPointF(
                                             cpuWidget->width() / 2,
                                             cpuWidget->height() / 2))
                                         .toPoint();
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, clickPosition);
        QCOMPARE(root->property("isEditing").toBool(), true);
        QCOMPARE(detailLoader->property("active").toBool(), false);
        QVERIFY(config.open(QIODevice::ReadOnly));
        QCOMPARE(config.readAll(), configBeforeClick);
        QCOMPARE(qmlWarnings.count(), 0);
    }

    void cpuPopupClampsToEditingScreenAvailableGeometry()
    {
        ModelFixture fixture({
            {"cpu", "Cpu", 0, {}},
        });
        QQmlEngine engine;
        QSignalSpy qmlWarnings(&engine, &QQmlEngine::warnings);
        QQmlComponent editingWindowComponent(&engine);
        editingWindowComponent.setData(
            QByteArrayLiteral("import QtQuick.Window\nWindow { visible: false }"), QUrl());
        QVERIFY2(
            editingWindowComponent.status() == QQmlComponent::Ready,
            qPrintable(errorText(editingWindowComponent)));
        std::unique_ptr<QQuickWindow> editingWindow(
            qobject_cast<QQuickWindow*>(editingWindowComponent.create()));
        QVERIFY(editingWindow != nullptr);
        UI::WindowGeometry windowGeometry;
        QCOMPARE(windowGeometry.availableGeometry(nullptr), QRectF());
        QCOMPARE(
            windowGeometry.availableGeometry(editingWindow.get()),
            QRectF(editingWindow->screen()->availableGeometry()));

        auto* const viewModel = fixture.model
                                    .data(fixture.model.index(0), UI::WidgetModel::ViewModelRole)
                                    .value<Widgets::WidgetViewModel*>();
        QVERIFY(viewModel != nullptr);
        QQmlComponent popupComponent(
            &engine, QUrl(QStringLiteral("qrc:/qt/qml/StatusBar/CpuDetailPopup.qml")));
        QVERIFY2(
            popupComponent.status() == QQmlComponent::Ready,
            qPrintable(errorText(popupComponent)));
        std::unique_ptr<QObject> popup(popupComponent.createWithInitialProperties({
            {QStringLiteral("viewModel"), QVariant::fromValue(viewModel)},
            {QStringLiteral("editingWindow"), QVariant::fromValue(editingWindow.get())},
        }));
        QVERIFY2(popup != nullptr, qPrintable(errorText(popupComponent)));

        QVariant position;
        QVERIFY(QMetaObject::invokeMethod(
            popup.get(),
            "clampedPosition",
            Q_RETURN_ARG(QVariant, position),
            Q_ARG(QVariant, -2500.0),
            Q_ARG(QVariant, -1400.0),
            Q_ARG(QVariant, QRectF(-1920.0, -1080.0, 1280.0, 1024.0))));
        QCOMPARE(position.toPointF(), QPointF(-1908.0, -1068.0));

        QVERIFY(QMetaObject::invokeMethod(
            popup.get(),
            "clampedPosition",
            Q_RETURN_ARG(QVariant, position),
            Q_ARG(QVariant, 3000.0),
            Q_ARG(QVariant, 900.0),
            Q_ARG(QVariant, QRectF(1920.0, 100.0, 800.0, 600.0))));
        QCOMPARE(position.toPointF(), QPointF(2388.0, 358.0));

        QVERIFY(popup->setProperty("width", 1000));
        QVERIFY(popup->setProperty("height", 700));
        QVERIFY(QMetaObject::invokeMethod(
            popup.get(),
            "clampedPosition",
            Q_RETURN_ARG(QVariant, position),
            Q_ARG(QVariant, 2500.0),
            Q_ARG(QVariant, 500.0),
            Q_ARG(QVariant, QRectF(1920.0, 100.0, 800.0, 600.0))));
        QCOMPARE(position.toPointF(), QPointF(1920.0, 100.0));
        QCOMPARE(qmlWarnings.count(), 0);
    }

    void widgetHostCoalescesPropertyChangesIntoOneLoad()
    {
        ModelFixture fixture({
            {"clock", "Clock", 0, {}},
            {"cpu", "Cpu", 3, {}},
        });
        QQmlEngine engine;
        QSignalSpy qmlWarnings(&engine, &QQmlEngine::warnings);
        QQmlComponent editingWindowComponent(&engine);
        editingWindowComponent.setData(
            QByteArrayLiteral("import QtQuick.Window\nWindow { visible: false }"), QUrl());
        QVERIFY2(
            editingWindowComponent.status() == QQmlComponent::Ready,
            qPrintable(errorText(editingWindowComponent)));
        std::unique_ptr<QQuickWindow> firstWindow(
            qobject_cast<QQuickWindow*>(editingWindowComponent.create()));
        std::unique_ptr<QQuickWindow> secondWindow(
            qobject_cast<QQuickWindow*>(editingWindowComponent.create()));
        QVERIFY(firstWindow != nullptr);
        QVERIFY(secondWindow != nullptr);

        const auto clockIndex = fixture.model.index(0);
        const auto cpuIndex = fixture.model.index(1);
        const QUrl clockUrl = fixture.model.data(
            clockIndex, UI::WidgetModel::QmlUrlRole).toUrl();
        const QUrl cpuUrl = fixture.model.data(cpuIndex, UI::WidgetModel::QmlUrlRole).toUrl();
        auto* const clockViewModel = fixture.model
                                         .data(clockIndex, UI::WidgetModel::ViewModelRole)
                                         .value<Widgets::WidgetViewModel*>();
        auto* const cpuViewModel = fixture.model
                                       .data(cpuIndex, UI::WidgetModel::ViewModelRole)
                                       .value<Widgets::WidgetViewModel*>();
        QVERIFY(clockViewModel != nullptr);
        QVERIFY(cpuViewModel != nullptr);

        QQmlComponent hostComponent(
            &engine, QUrl(QStringLiteral("qrc:/qt/qml/StatusBar/WidgetHost.qml")));
        QVERIFY2(
            hostComponent.status() == QQmlComponent::Ready,
            qPrintable(errorText(hostComponent)));
        std::unique_ptr<QObject> host(hostComponent.createWithInitialProperties({
            {QStringLiteral("qmlUrl"), QVariant::fromValue(clockUrl)},
            {QStringLiteral("viewModel"), QVariant::fromValue(clockViewModel)},
            {QStringLiteral("editingWindow"), QVariant::fromValue(firstWindow.get())},
            {QStringLiteral("editing"), false},
        }));
        QVERIFY2(host != nullptr, qPrintable(errorText(hostComponent)));
        QObject* const loader = host->findChild<QObject*>(QStringLiteral("widgetLoader"));
        QVERIFY(loader != nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(loader->property("item").value<QObject*>() != nullptr, 1000);
        QObject* const initialWidget = loader->property("item").value<QObject*>();
        QCOMPARE(
            initialWidget->property("viewModel").value<Widgets::WidgetViewModel*>(),
            clockViewModel);
        QCOMPARE(
            initialWidget->property("editingWindow").value<QQuickWindow*>(),
            firstWindow.get());

        QSignalSpy loaded(loader, SIGNAL(loaded()));
        QVERIFY(loaded.isValid());

        QVERIFY(host->setProperty("qmlUrl", QVariant::fromValue(cpuUrl)));
        QVERIFY(host->setProperty("viewModel", QVariant::fromValue(cpuViewModel)));
        QVERIFY(host->setProperty("editingWindow", QVariant::fromValue(secondWindow.get())));

        QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 1, 1000);
        QObject* const widget = loader->property("item").value<QObject*>();
        QCOMPARE(
            widget->property("viewModel").value<Widgets::WidgetViewModel*>(),
            cpuViewModel);
        QCOMPARE(
            widget->property("editingWindow").value<QQuickWindow*>(),
            secondWindow.get());
        QCOMPARE(loaded.count(), 1);
        QCoreApplication::sendPostedEvents();
        QCoreApplication::processEvents();
        QCOMPARE(loaded.count(), 1);
        QCOMPARE(qmlWarnings.count(), 0);
    }

    void cpuControlsExposeAccessibleKeyboardActions_data()
    {
        QTest::addColumn<int>("activationKey");
        QTest::newRow("return") << static_cast<int>(Qt::Key_Return);
        QTest::newRow("enter") << static_cast<int>(Qt::Key_Enter);
        QTest::newRow("space") << static_cast<int>(Qt::Key_Space);
    }

    void cpuControlsExposeAccessibleKeyboardActions()
    {
        QFETCH(int, activationKey);
        ModelFixture fixture({
            {"cpu", "Cpu", 0, {}},
        });
        QQmlApplicationEngine engine;
        QSignalSpy qmlWarnings(&engine, &QQmlEngine::warnings);
        engine.setInitialProperties({
            {QStringLiteral("widgetModel"), QVariant::fromValue(&fixture.model)},
        });
        engine.loadFromModule(QStringLiteral("StatusBar"), QStringLiteral("Main"));

        QCOMPARE(engine.rootObjects().size(), 1);
        auto* const window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        QVERIFY(window != nullptr);
        window->show();
        QTRY_VERIFY_WITH_TIMEOUT(window->isVisible(), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(
            findQuickItem(window->contentItem(), QStringLiteral("widgetLoader")) != nullptr,
            1000);
        QObject* const loader = findQuickItem(
            window->contentItem(), QStringLiteral("widgetLoader"));
        QTRY_VERIFY_WITH_TIMEOUT(loader->property("item").value<QObject*>() != nullptr, 1000);
        auto* const cpuWidget = qobject_cast<QQuickItem*>(
            loader->property("item").value<QObject*>());
        QVERIFY(cpuWidget != nullptr);
        QObject* const detailLoader = cpuWidget->findChild<QObject*>(
            QStringLiteral("cpuDetailLoader"));
        QVERIFY(detailLoader != nullptr);
        QCOMPARE(detailLoader->property("active").toBool(), false);

        cpuWidget->forceActiveFocus(Qt::TabFocusReason);
        QVERIFY(cpuWidget->hasActiveFocus());
        QTest::keyClick(window, static_cast<Qt::Key>(activationKey));
        QTRY_COMPARE_WITH_TIMEOUT(detailLoader->property("active").toBool(), true, 1000);
        QCOMPARE(cpuWidget->property("activeFocusOnTab").toBool(), true);
        QAccessibleInterface* const cpuAccessible = QAccessible::queryAccessibleInterface(cpuWidget);
        QVERIFY(cpuAccessible != nullptr);
        QCOMPARE(cpuAccessible->role(), QAccessible::Button);
        QCOMPARE(cpuAccessible->text(QAccessible::Name), QStringLiteral("CPU details"));
        QVERIFY(cpuAccessible->state().checkable);
        QVERIFY(cpuAccessible->state().checked);

        auto* const popup = qobject_cast<QQuickWindow*>(
            detailLoader->property("item").value<QObject*>());
        QVERIFY(popup != nullptr);
        QQuickItem* const pinButton = findQuickItem(
            popup->contentItem(), QStringLiteral("cpuPinButton"));
        QVERIFY(pinButton != nullptr);
        QCOMPARE(pinButton->property("activeFocusOnTab").toBool(), true);
        QAccessibleInterface* const pinAccessible = QAccessible::queryAccessibleInterface(pinButton);
        QVERIFY(pinAccessible != nullptr);
        QCOMPARE(pinAccessible->role(), QAccessible::Button);
        QCOMPARE(pinAccessible->text(QAccessible::Name), QStringLiteral("Pin CPU details"));
        QVERIFY(pinAccessible->state().checkable);
        QVERIFY(!pinAccessible->state().checked);

        pinButton->forceActiveFocus(Qt::TabFocusReason);
        QVERIFY(pinButton->hasActiveFocus());
        QTest::keyClick(popup, static_cast<Qt::Key>(activationKey));
        QCOMPARE(popup->property("isPinned").toBool(), true);
        QVERIFY(pinAccessible->state().checked);
        QCOMPARE(qmlWarnings.count(), 0);
    }
};

QTEST_MAIN(ResourceEmbeddingTest)
#include "resource_embedding_test.moc"
