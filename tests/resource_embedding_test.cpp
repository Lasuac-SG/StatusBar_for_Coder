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
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QStyleHints>
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

struct ModelFixture final {
    QTemporaryDir directory;
    Platform::CpuService cpuService{std::make_unique<NullCpuDataSource>()};
    Widgets::WidgetContext context{cpuService};
    std::unique_ptr<UI::WidgetModel> model;
    Core::Result<void> setupResult{
        Core::Result<void>::failure(QStringLiteral("Fixture setup did not run"))};

    explicit ModelFixture(QList<Core::WidgetConfig> widgets)
    {
        if (!directory.isValid()) {
            setupResult = Core::Result<void>::failure(
                QStringLiteral("Could not create fixture temporary directory"));
            return;
        }

        auto registryResult = Widgets::registerAllWidgets();
        if (!registryResult.hasValue()) {
            setupResult = Core::Result<void>::failure(
                QStringLiteral("Could not create widget registry: %1")
                    .arg(registryResult.error()));
            return;
        }

        Core::ConfigRepository repository(directory.filePath(QStringLiteral("config.json")));
        const auto saveResult = repository.save(
            Core::ConfigDocument{1, std::move(widgets)});
        if (!saveResult.hasValue()) {
            setupResult = Core::Result<void>::failure(
                QStringLiteral("Could not save fixture configuration: %1")
                    .arg(saveResult.error()));
            return;
        }

        model = std::make_unique<UI::WidgetModel>(
            repository, std::move(registryResult).value(), context);
        const auto loadResult = model->loadFromConfig();
        if (!loadResult.hasValue()) {
            setupResult = Core::Result<void>::failure(
                QStringLiteral("Could not load fixture configuration: %1")
                    .arg(loadResult.error()));
            model.reset();
            return;
        }

        setupResult = Core::Result<void>::success();
    }

    [[nodiscard]] bool isReady() const noexcept
    {
        return setupResult.hasValue() && model != nullptr;
    }

    [[nodiscard]] QString error() const
    {
        return setupResult.hasValue()
            ? QStringLiteral("Fixture model was not created")
            : setupResult.error();
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
        QCOMPARE(registry.descriptors().size(), std::size_t{2});
        QVERIFY(registry.find(QStringLiteral("Clock")) != nullptr);
        QVERIFY(registry.find(QStringLiteral("Cpu")) != nullptr);
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
        QVERIFY2(fixture.isReady(), qPrintable(fixture.error()));
        QCOMPARE(fixture.model->rowCount(), 0);
        QQmlApplicationEngine engine;
        QSignalSpy qmlWarnings(&engine, &QQmlEngine::warnings);
        QVERIFY(!engine.rootContext()->contextProperty(QStringLiteral("widgetModel")).isValid());
        QVERIFY(!engine.rootContext()->contextProperty(QStringLiteral("clockAdapter")).isValid());
        QVERIFY(!engine.rootContext()->contextProperty(QStringLiteral("cpuAdapter")).isValid());

        engine.setInitialProperties({
            {QStringLiteral("widgetModel"), QVariant::fromValue(fixture.model.get())},
        });
        engine.loadFromModule(QStringLiteral("StatusBar"), QStringLiteral("Main"));

        QCOMPARE(engine.rootObjects().size(), 1);
        auto* const root = engine.rootObjects().constFirst();
        auto* const window = qobject_cast<QQuickWindow*>(root);
        QVERIFY(window != nullptr);
        QVERIFY(!window->isVisible());
        QCOMPARE(root->property("widgetModel").value<UI::WidgetModel*>(), fixture.model.get());
        QCOMPARE(qmlWarnings.count(), 0);
    }

    void descriptorComponentsUseTypedViewModelsAndCpuDetailIsLazy()
    {
        ModelFixture fixture({
            {"clock", "Clock", 0, {}},
            {"cpu", "Cpu", 3, {}},
        });
        QVERIFY2(fixture.isReady(), qPrintable(fixture.error()));
        QCOMPARE(fixture.model->rowCount(), 2);
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

        int createdComponentCount = 0;
        for (int row = 0; row < fixture.model->rowCount(); ++row) {
            const auto index = fixture.model->index(row);
            const QUrl qmlUrl = fixture.model->data(index, UI::WidgetModel::QmlUrlRole).toUrl();
            auto* const viewModel = fixture.model
                                        ->data(index, UI::WidgetModel::ViewModelRole)
                                        .value<Widgets::WidgetViewModel*>();
            QVERIFY(viewModel != nullptr);
            QQmlComponent component(&engine, qmlUrl);
            QVERIFY2(component.status() == QQmlComponent::Ready, qPrintable(errorText(component)));
            auto object = createWidgetObject(component, viewModel, editingWindow.get());
            QVERIFY2(object != nullptr, qPrintable(errorText(component)));
            ++createdComponentCount;
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
        QCOMPARE(createdComponentCount, 2);
    }

    void widgetHostPreservesTypedInitialProperties()
    {
        ModelFixture fixture({
            {"clock", "Clock", 0, {}},
            {"cpu", "Cpu", 3, {}},
        });
        QVERIFY2(fixture.isReady(), qPrintable(fixture.error()));
        QCOMPARE(fixture.model->rowCount(), 2);
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

        for (int row = 0; row < fixture.model->rowCount(); ++row) {
            const auto index = fixture.model->index(row);
            const QUrl qmlUrl = fixture.model->data(index, UI::WidgetModel::QmlUrlRole).toUrl();
            auto* const viewModel = fixture.model
                                        ->data(index, UI::WidgetModel::ViewModelRole)
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
        QVERIFY2(fixture.isReady(), qPrintable(fixture.error()));
        QCOMPARE(fixture.model->rowCount(), 1);
        QQmlApplicationEngine engine;
        QSignalSpy qmlWarnings(&engine, &QQmlEngine::warnings);
        engine.setInitialProperties({
            {QStringLiteral("widgetModel"), QVariant::fromValue(fixture.model.get())},
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

        QFile config(fixture.directory.filePath(QStringLiteral("config.json")));
        QVERIFY(config.open(QIODevice::ReadOnly));
        const QByteArray configBeforeHold = config.readAll();
        config.close();

        window->show();
        QTRY_VERIFY_WITH_TIMEOUT(window->isVisible(), 1000);
        QQuickItem* const interactionArea = findQuickItem(
            cpuWidget, QStringLiteral("cpuInteractionArea"));
        QVERIFY(interactionArea != nullptr);
        QVERIFY(interactionArea->width() > 0);
        QVERIFY(interactionArea->height() > 0);

        QSignalSpy editingChanged(root, SIGNAL(isEditingChanged()));
        QSignalSpy modelChanged(fixture.model.get(), &QAbstractItemModel::dataChanged);
        QVERIFY(editingChanged.isValid());
        QVERIFY(modelChanged.isValid());

        const int holdInterval = QGuiApplication::styleHints()->mousePressAndHoldInterval();
        QVERIFY(holdInterval > 0);
        QVERIFY2(holdInterval <= 2000, "Platform press-and-hold interval is unexpectedly long");
        QCOMPARE(interactionArea->property("pressAndHoldInterval").toInt(), holdInterval);
        const QPoint holdPosition = interactionArea
                                        ->mapToScene(QPointF(
                                            interactionArea->width() / 2,
                                            interactionArea->height() / 2))
                                        .toPoint();

        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, holdPosition);
        QVERIFY(interactionArea->property("pressed").toBool());
        QTest::qWait(holdInterval + 500);
        const bool stillPressedAfterHold = interactionArea->property("pressed").toBool();
        const bool editingDuringHold = root->property("isEditing").toBool();
        const auto editingChangesDuringHold = editingChanged.count();
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, holdPosition);

        QVERIFY(stillPressedAfterHold);
        QVERIFY(editingDuringHold);
        QCOMPARE(editingChangesDuringHold, qsizetype{1});
        QCOMPARE(editingChanged.count(), qsizetype{1});
        QCOMPARE(root->property("isEditing").toBool(), true);
        QCOMPARE(detailLoader->property("active").toBool(), false);
        QCOMPARE(modelChanged.count(), 0);
        QCoreApplication::processEvents();
        QCOMPARE(root->property("isEditing").toBool(), true);
        QCOMPARE(editingChanged.count(), 1);
        QVERIFY(config.open(QIODevice::ReadOnly));
        QCOMPARE(config.readAll(), configBeforeHold);
        QCOMPARE(qmlWarnings.count(), 0);
    }

    void cpuPopupClampsToEditingScreenAvailableGeometry()
    {
        ModelFixture fixture({
            {"cpu", "Cpu", 0, {}},
        });
        QVERIFY2(fixture.isReady(), qPrintable(fixture.error()));
        QCOMPARE(fixture.model->rowCount(), 1);
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
                                    ->data(fixture.model->index(0), UI::WidgetModel::ViewModelRole)
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
        QVERIFY2(fixture.isReady(), qPrintable(fixture.error()));
        QCOMPARE(fixture.model->rowCount(), 2);
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

        const auto clockIndex = fixture.model->index(0);
        const auto cpuIndex = fixture.model->index(1);
        const QUrl clockUrl = fixture.model->data(
            clockIndex, UI::WidgetModel::QmlUrlRole).toUrl();
        const QUrl cpuUrl = fixture.model->data(cpuIndex, UI::WidgetModel::QmlUrlRole).toUrl();
        auto* const clockViewModel = fixture.model
                                         ->data(clockIndex, UI::WidgetModel::ViewModelRole)
                                         .value<Widgets::WidgetViewModel*>();
        auto* const cpuViewModel = fixture.model
                                       ->data(cpuIndex, UI::WidgetModel::ViewModelRole)
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
        QVERIFY2(fixture.isReady(), qPrintable(fixture.error()));
        QCOMPARE(fixture.model->rowCount(), 1);
        QQmlApplicationEngine engine;
        QSignalSpy qmlWarnings(&engine, &QQmlEngine::warnings);
        engine.setInitialProperties({
            {QStringLiteral("widgetModel"), QVariant::fromValue(fixture.model.get())},
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
