#include "ui/qt_application.h"

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QEventLoop>
#include <QFont>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickWindow>
#include <QUrl>
#include <QVariant>

#include <cstdlib>
#include <utility>

namespace UI {

namespace {

constexpr int maximumInitialQmlEventTurns = 32;
constexpr int loaderNull = 0;
constexpr int loaderReady = 1;
constexpr int loaderLoading = 2;
constexpr int loaderError = 3;

void collectWidgetLoaders(
    QQuickItem* const item,
    QList<QQuickItem*>& loaders)
{
    if (item->objectName() == QStringLiteral("widgetLoader")) {
        loaders.append(item);
    }
    for (QQuickItem* const child : item->childItems()) {
        collectWidgetLoaders(child, loaders);
    }
}

} // namespace

QtApplication::QtApplication(
    int& argc,
    char** argv,
    Core::ConfigRepository repository,
    Widgets::WidgetRegistry registry,
    std::unique_ptr<Platform::CpuDataSource> cpuDataSource,
    QmlEntryPoint qmlEntryPoint)
    : application_(argc, argv)
    , cpuService_(std::move(cpuDataSource))
    , widgetContext_{cpuService_}
    , configPath_(repository.path())
    , widgetModel_(std::move(repository), std::move(registry), widgetContext_)
    , qmlEntryPoint_(std::move(qmlEntryPoint))
{
    QFont defaultFont(QStringLiteral("Segoe UI"));
    defaultFont.setStyleStrategy(QFont::PreferAntialias);
    application_.setFont(defaultFont);

    QObject::connect(
        &engine_,
        &QQmlEngine::warnings,
        &engine_,
        [this](const QList<QQmlError>& warnings) {
            for (const auto& warning : warnings) {
                qmlDiagnostics_.append(warning.toString());
            }
        });
    QObject::connect(
        &engine_,
        &QQmlApplicationEngine::objectCreationFailed,
        &engine_,
        [this](const QUrl& url) {
            objectCreationFailed_ = true;
            qmlDiagnostics_.append(
                QStringLiteral("Object creation failed for %1").arg(url.toString()));
        });
}

QtApplication::~QtApplication() = default;

Core::Result<void> QtApplication::initialize()
{
    if (state_ == State::Initialized) {
        return Core::Result<void>::success();
    }
    if (state_ == State::Failed) {
        return Core::Result<void>::failure(
            QStringLiteral("QtApplication initialization previously failed: %1")
                .arg(failure_));
    }

    const auto modelResult = widgetModel_.loadFromConfig();
    if (!modelResult.hasValue()) {
        return failInitialization(
            QStringLiteral("Failed to load configuration from %1: %2")
                .arg(configPath_, modelResult.error()));
    }

    qmlDiagnostics_.clear();
    objectCreationFailed_ = false;
    engine_.setInitialProperties({
        {QStringLiteral("widgetModel"), QVariant::fromValue(&widgetModel_)},
    });
    engine_.loadFromModule(qmlEntryPoint_.moduleUri, qmlEntryPoint_.typeName);

    if (objectCreationFailed_ || !qmlDiagnostics_.isEmpty()) {
        return failInitialization(
            QStringLiteral("Failed to load QML root %1.%2: %3")
                .arg(
                    qmlEntryPoint_.moduleUri,
                    qmlEntryPoint_.typeName,
                    qmlDiagnostics_.join(QLatin1Char('\n'))));
    }

    const QList<QObject*> roots = engine_.rootObjects();
    if (roots.size() != 1) {
        return failInitialization(
            QStringLiteral("Failed to load QML root %1.%2: expected exactly one root object, got %3")
                .arg(qmlEntryPoint_.moduleUri, qmlEntryPoint_.typeName)
                .arg(roots.size()));
    }

    auto* const rootWindow = qobject_cast<QQuickWindow*>(roots.constFirst());
    if (rootWindow == nullptr) {
        return failInitialization(
            QStringLiteral("Failed to load QML root %1.%2: root object is not a QQuickWindow")
                .arg(qmlEntryPoint_.moduleUri, qmlEntryPoint_.typeName));
    }
    if (rootWindow->isVisible()) {
        return failInitialization(
            QStringLiteral("Failed to load QML root %1.%2: root window must start hidden")
                .arg(qmlEntryPoint_.moduleUri, qmlEntryPoint_.typeName));
    }

    const auto readyResult = awaitInitialQmlReady(rootWindow);
    if (!readyResult.hasValue()) {
        return failInitialization(
            QStringLiteral("Failed to load QML root %1.%2: %3")
                .arg(
                    qmlEntryPoint_.moduleUri,
                    qmlEntryPoint_.typeName,
                    readyResult.error()));
    }

    rootWindow_ = rootWindow;
    state_ = State::Initialized;
    return Core::Result<void>::success();
}

QQuickWindow* QtApplication::window() const noexcept
{
    return rootWindow_.data();
}

Core::Result<void> QtApplication::show()
{
    if (state_ != State::Initialized || rootWindow_ == nullptr) {
        return Core::Result<void>::failure(
            QStringLiteral("Cannot show Qt application before initialize() succeeds"));
    }

    rootWindow_->show();
    return Core::Result<void>::success();
}

int QtApplication::run()
{
    if (state_ != State::Initialized || rootWindow_ == nullptr) {
        return EXIT_FAILURE;
    }
    return application_.exec();
}

void QtApplication::quit() noexcept
{
    application_.quit();
}

Core::Result<void> QtApplication::failInitialization(QString error)
{
    discardRootObjects();
    widgetModel_.clear();
    failure_ = std::move(error);
    state_ = State::Failed;
    return Core::Result<void>::failure(failure_);
}

Core::Result<void> QtApplication::awaitInitialQmlReady(QQuickWindow* const rootWindow)
{
    QPointer<QQuickWindow> guardedRoot(rootWindow);
    int consecutiveReadyTurns = 0;

    for (int turn = 0; turn < maximumInitialQmlEventTurns; ++turn) {
        QCoreApplication::processEvents(
            QEventLoop::AllEvents, QDeadlineTimer(10));

        if (guardedRoot == nullptr) {
            return Core::Result<void>::failure(
                QStringLiteral("root window was destroyed during initial QML loading"));
        }
        if (objectCreationFailed_ || !qmlDiagnostics_.isEmpty()) {
            return Core::Result<void>::failure(
                qmlDiagnostics_.isEmpty()
                    ? QStringLiteral("QML object creation failed")
                    : qmlDiagnostics_.join(QLatin1Char('\n')));
        }

        QQuickItem* const contentItem = guardedRoot->contentItem();
        if (contentItem == nullptr) {
            return Core::Result<void>::failure(
                QStringLiteral("root window has no content item"));
        }

        QList<QQuickItem*> loaders;
        collectWidgetLoaders(contentItem, loaders);
        bool allLoadersReady = loaders.size() == widgetModel_.rowCount();
        for (QQuickItem* const loader : loaders) {
            const QVariant statusValue = loader->property("status");
            if (!statusValue.isValid()) {
                return Core::Result<void>::failure(
                    QStringLiteral("widget loader has no status property"));
            }

            const int status = statusValue.toInt();
            if (status == loaderError) {
                return Core::Result<void>::failure(
                    QStringLiteral("configured widget loader reported an error"));
            }
            if (status == loaderNull || status == loaderLoading) {
                allLoadersReady = false;
            } else if (status != loaderReady) {
                return Core::Result<void>::failure(
                    QStringLiteral("configured widget loader reported unknown status %1")
                        .arg(status));
            }
        }

        if (allLoadersReady) {
            // The second ready turn catches work posted by a widget's completion handlers.
            ++consecutiveReadyTurns;
            if (consecutiveReadyTurns == 2) {
                return Core::Result<void>::success();
            }
        } else {
            consecutiveReadyTurns = 0;
        }
    }

    return Core::Result<void>::failure(
        QStringLiteral(
            "configured widgets did not become ready within %1 event turns "
            "(expected %2 loaders)")
            .arg(maximumInitialQmlEventTurns)
            .arg(widgetModel_.rowCount()));
}

void QtApplication::discardRootObjects() noexcept
{
    rootWindow_.clear();
    const QList<QObject*> roots = engine_.rootObjects();
    for (QObject* const root : roots) {
        delete root;
    }
}

} // namespace UI
