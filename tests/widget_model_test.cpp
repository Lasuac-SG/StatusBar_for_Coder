#include "core/config_repository.h"
#include "core/internal/config_repository_operations.h"
#include "platform/cpu_service.h"
#include "ui/widget_model.h"
#include "widgets/registry_setup.h"
#include "widgets/widget_descriptor.h"
#include "widgets/widget_registry.h"
#include "widgets/widget_view_model.h"

#include <QtTest>

#include <QFile>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>

#include <memory>
#include <utility>
#include <vector>

namespace {

class NullCpuDataSource final : public Platform::CpuDataSource {
public:
    std::optional<Platform::CpuTimes> sampleTimes() noexcept override
    {
        ++timesCalls;
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

    int timesCalls{};
};

class TrackingOperations final : public Core::Internal::ConfigRepositoryOperations {
public:
    TrackingOperations()
        : delegate(Core::Internal::defaultConfigRepositoryOperations())
    {
    }

    Core::Internal::LockAttemptResult attemptMigrationLock(
        QLockFile& lock,
        int timeoutMs) override
    {
        return delegate->attemptMigrationLock(lock, timeoutMs);
    }

    Core::Result<void> atomicWrite(const QString& path, const QByteArray& bytes) override
    {
        ++writeAttempts;
        if (failWrites) {
            return Core::Result<void>::failure(QStringLiteral("Injected save failure"));
        }
        return delegate->atomicWrite(path, bytes);
    }

    std::shared_ptr<Core::Internal::ConfigRepositoryOperations> delegate;
    int writeAttempts{};
    bool failWrites{};
};

class NullFactoryViewModel final : public Widgets::WidgetViewModel {
public:
    explicit NullFactoryViewModel(QString id)
        : m_id(std::move(id))
    {
    }
    [[nodiscard]] const QString& instanceId() const noexcept override { return m_id; }

private:
    QString m_id;
};

Widgets::WidgetRegistry builtInRegistry()
{
    auto result = Widgets::registerAllWidgets();
    Q_ASSERT(result.hasValue());
    return std::move(result).value();
}

bool writeBytes(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

QByteArray readBytes(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

QObject* viewModelAt(const UI::WidgetModel& model, int row)
{
    return model.data(model.index(row), UI::WidgetModel::ViewModelRole).value<QObject*>();
}

} // namespace

class WidgetModelTest final : public QObject {
    Q_OBJECT

private slots:
    void cpuServiceDemandTracksModelReloadsWithoutTypeDispatch()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Core::ConfigRepository repository(directory.filePath("config.json"));
        QVERIFY(repository.save(Core::ConfigDocument{}).hasValue());
        auto source = std::make_unique<NullCpuDataSource>();
        auto* const counts = source.get();
        Platform::CpuService cpuService(std::move(source));
        auto* const timer = cpuService.findChild<QTimer*>();
        QVERIFY(timer != nullptr);
        Widgets::WidgetContext context{cpuService};
        UI::WidgetModel model(repository, builtInRegistry(), context);

        QVERIFY(model.loadFromConfig().hasValue());
        QVERIFY(!timer->isActive());
        QCOMPARE(counts->timesCalls, 0);

        QVERIFY(repository.save(Core::ConfigDocument{
                                    1,
                                    {{"clock", "Clock", 0, {}}},
                                })
                    .hasValue());
        QVERIFY(model.loadFromConfig().hasValue());
        QVERIFY(!timer->isActive());
        QCOMPARE(counts->timesCalls, 0);

        QVERIFY(repository.save(Core::ConfigDocument{
                                    1,
                                    {
                                        {"cpu-one", "Cpu", 0, {}},
                                        {"cpu-two", "Cpu", 2, {}},
                                    },
                                })
                    .hasValue());
        QVERIFY(model.loadFromConfig().hasValue());
        QVERIFY(timer->isActive());
        QCOMPARE(counts->timesCalls, 1);

        QVERIFY(repository.save(Core::ConfigDocument{
                                    1,
                                    {{"clock-again", "Clock", 0, {}}},
                                })
                    .hasValue());
        QVERIFY(model.loadFromConfig().hasValue());
        QVERIFY(!timer->isActive());
        QCOMPARE(counts->timesCalls, 1);

        QVERIFY(repository.save(Core::ConfigDocument{
                                    1,
                                    {{"cpu-again", "Cpu", 0, {}}},
                                })
                    .hasValue());
        QVERIFY(model.loadFromConfig().hasValue());
        QVERIFY(timer->isActive());
        QCOMPARE(counts->timesCalls, 2);
    }

    void exposesExactRolesAndDescriptorDerivedData()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Core::ConfigRepository repository(directory.filePath("config.json"));
        const Core::ConfigDocument document{
            1,
            {
                {"clock-id", "Clock", 1, QJsonObject{{"format", "HH:mm"}}},
                {"cpu-id", "Cpu", 5, {}},
            },
        };
        QVERIFY(repository.save(document).hasValue());
        Platform::CpuService cpuService(std::make_unique<NullCpuDataSource>());
        Widgets::WidgetContext context{cpuService};
        UI::WidgetModel model(repository, builtInRegistry(), context);

        const auto loaded = model.loadFromConfig();
        QVERIFY2(loaded.hasValue(), qPrintable(loaded.error()));
        const QHash<int, QByteArray> expectedRoles{
            {UI::WidgetModel::InstanceIdRole, "instanceId"},
            {UI::WidgetModel::TypeRole, "type"},
            {UI::WidgetModel::SlotRole, "slot"},
            {UI::WidgetModel::SpanRole, "span"},
            {UI::WidgetModel::QmlUrlRole, "qmlUrl"},
            {UI::WidgetModel::ViewModelRole, "viewModel"},
        };
        QCOMPARE(model.roleNames(), expectedRoles);
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.data(model.index(0), UI::WidgetModel::InstanceIdRole).toString(),
                 QString("clock-id"));
        QCOMPARE(model.data(model.index(0), UI::WidgetModel::TypeRole).toString(),
                 QString("Clock"));
        QCOMPARE(model.data(model.index(0), UI::WidgetModel::SlotRole).toInt(), 1);
        QCOMPARE(model.data(model.index(0), UI::WidgetModel::SpanRole).toInt(), 3);
        QCOMPARE(model.data(model.index(0), UI::WidgetModel::QmlUrlRole).toUrl(),
                 QUrl("qrc:/qt/qml/StatusBar/ClockWidget.qml"));
        QCOMPARE(viewModelAt(model, 0)->property("instanceId").toString(), QString("clock-id"));
        QCOMPARE(model.data(model.index(1), UI::WidgetModel::SpanRole).toInt(), 2);
        QCOMPARE(model.data(model.index(1), UI::WidgetModel::QmlUrlRole).toUrl(),
                 QUrl("qrc:/qt/qml/StatusBar/CpuWidget.qml"));
        QVERIFY(!model.data(QModelIndex{}, UI::WidgetModel::TypeRole).isValid());
        QVERIFY(!model.data(model.index(9), UI::WidgetModel::TypeRole).isValid());
    }

    void sameTypeConfigsCreateDistinctObjectsWithPersistentIds()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Core::ConfigRepository repository(directory.filePath("config.json"));
        QVERIFY(repository.save(Core::ConfigDocument{
                                    1,
                                    {
                                        {"left-clock", "Clock", 0, {}},
                                        {"right-clock", "Clock", 4, {}},
                                    },
                                })
                    .hasValue());
        Platform::CpuService cpuService(std::make_unique<NullCpuDataSource>());
        Widgets::WidgetContext context{cpuService};
        UI::WidgetModel model(repository, builtInRegistry(), context);

        QVERIFY(model.loadFromConfig().hasValue());
        QObject* const first = viewModelAt(model, 0);
        QObject* const second = viewModelAt(model, 1);
        QVERIFY(first != nullptr);
        QVERIFY(second != nullptr);
        QVERIFY(first != second);
        QCOMPARE(model.data(model.index(0), UI::WidgetModel::InstanceIdRole).toString(),
                 QString("left-clock"));
        QCOMPARE(model.data(model.index(1), UI::WidgetModel::InstanceIdRole).toString(),
                 QString("right-clock"));
        QCOMPARE(model.data(model.index(0), UI::WidgetModel::TypeRole).toString(),
                 model.data(model.index(1), UI::WidgetModel::TypeRole).toString());
    }

    void unknownConfigsRemainFieldEquivalentAndOrderedAfterKnownMove()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Core::ConfigRepository repository(directory.filePath("config.json"));
        const Core::WidgetConfig firstUnknown{
            "future-one",
            "FutureWidget",
            8,
            QJsonObject{{"nested", QJsonObject{{"keep", true}}}},
            QJsonObject{{"futurePayload", QJsonObject{{"opaque", true}}}},
        };
        const Core::WidgetConfig known{
            "clock-id",
            "Clock",
            0,
            QJsonObject{{"timeZone", "UTC"}},
            QJsonObject{{"knownExtra", QJsonArray{1, 2, 3}}},
        };
        const Core::WidgetConfig secondUnknown{
            "future-two",
            "AnotherFutureWidget",
            12,
            QJsonObject{{"value", 42}},
            QJsonObject{{"secondExtra", QJsonObject{{"nested", "keep"}}}},
        };
        const QJsonObject rootExtensions{
            {"metadata", QJsonObject{{"owner", "future"}, {"nested", QJsonArray{true, 7}}}},
        };
        QVERIFY(repository
                    .save(Core::ConfigDocument{
                        1, {firstUnknown, known, secondUnknown}, rootExtensions})
                    .hasValue());
        Platform::CpuService cpuService(std::make_unique<NullCpuDataSource>());
        Widgets::WidgetContext context{cpuService};
        UI::WidgetModel model(repository, builtInRegistry(), context);

        QVERIFY(model.loadFromConfig().hasValue());
        QCOMPARE(model.rowCount(), 1);
        QVERIFY(model.setTotalSlots(9));
        QVERIFY(model.dropWidget(QStringLiteral("clock-id"), 4));

        const auto reloaded = repository.load();
        QVERIFY2(reloaded.hasValue(), qPrintable(reloaded.error()));
        QCOMPARE(reloaded.value().version, 1);
        QCOMPARE(reloaded.value().extensions, rootExtensions);
        QCOMPARE(reloaded.value().widgets.size(), 3);
        QVERIFY(reloaded.value().widgets.at(0) == firstUnknown);
        Core::WidgetConfig movedKnown = known;
        movedKnown.slot = 4;
        QVERIFY(reloaded.value().widgets.at(1) == movedKnown);
        QVERIFY(reloaded.value().widgets.at(2) == secondUnknown);
    }

    void shrinkingSlotsNormalizesOnceAndNoOpDoesNotSave()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath("config.json");
        auto operations = std::make_shared<TrackingOperations>();
        const auto repository = Core::Internal::makeConfigRepository(path, {}, operations);
        QVERIFY(repository.save(Core::ConfigDocument{
                                    1,
                                    {
                                        {"clock", "Clock", 8, {}},
                                        {"cpu", "Cpu", 1, {}},
                                    },
                                })
                    .hasValue());
        operations->writeAttempts = 0;
        Platform::CpuService cpuService(std::make_unique<NullCpuDataSource>());
        Widgets::WidgetContext context{cpuService};
        UI::WidgetModel model(repository, builtInRegistry(), context);
        QVERIFY(model.loadFromConfig().hasValue());
        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);

        QVERIFY(model.setTotalSlots(7));
        QCOMPARE(operations->writeAttempts, 1);
        QCOMPARE(model.data(model.index(0), UI::WidgetModel::SlotRole).toInt(), 4);
        QCOMPARE(model.data(model.index(1), UI::WidgetModel::SlotRole).toInt(), 1);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.at(0).at(0).value<QModelIndex>().row(), 0);
        QCOMPARE(changed.at(0).at(1).value<QModelIndex>().row(), 0);

        QVERIFY(model.setTotalSlots(7));
        QVERIFY(model.setTotalSlots(8));
        QCOMPARE(operations->writeAttempts, 1);
        QVERIFY(model.dropWidget(QStringLiteral("cpu"), 1));
        QCOMPARE(operations->writeAttempts, 1);
        QVERIFY(!model.setTotalSlots(4));
        QCOMPARE(operations->writeAttempts, 1);
        QCOMPARE(model.data(model.index(0), UI::WidgetModel::SlotRole).toInt(), 4);
        QCOMPARE(model.data(model.index(1), UI::WidgetModel::SlotRole).toInt(), 1);
    }

    void insufficientShrinkInvalidatesGeometryAndBlocksDrops()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath("config.json");
        auto operations = std::make_shared<TrackingOperations>();
        const auto repository = Core::Internal::makeConfigRepository(path, {}, operations);
        QVERIFY(repository.save(Core::ConfigDocument{
                                    1,
                                    {
                                        {"clock", "Clock", 0, {}},
                                        {"cpu", "Cpu", 5, {}},
                                    },
                                })
                    .hasValue());
        operations->writeAttempts = 0;
        Platform::CpuService cpuService(std::make_unique<NullCpuDataSource>());
        Widgets::WidgetContext context{cpuService};
        UI::WidgetModel model(repository, builtInRegistry(), context);
        QVERIFY(model.loadFromConfig().hasValue());
        QVERIFY(model.setTotalSlots(9));

        QVERIFY(!model.setTotalSlots(4));
        QCOMPARE(operations->writeAttempts, 0);
        QVERIFY(!model.dropWidget(QStringLiteral("clock"), 5));
        QCOMPARE(operations->writeAttempts, 0);
        QVERIFY2(model.lastError().contains("4"), qPrintable(model.lastError()));
        QCOMPARE(model.data(model.index(0), UI::WidgetModel::SlotRole).toInt(), 0);
        QCOMPARE(model.data(model.index(1), UI::WidgetModel::SlotRole).toInt(), 5);
    }

    void persistenceFailedResizeBlocksDropsAndRetriesSameGeometry()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath("config.json");
        auto operations = std::make_shared<TrackingOperations>();
        const auto repository = Core::Internal::makeConfigRepository(path, {}, operations);
        QVERIFY(repository.save(Core::ConfigDocument{
                                    1,
                                    {
                                        {"clock", "Clock", 6, {}},
                                        {"cpu", "Cpu", 1, {}},
                                    },
                                })
                    .hasValue());
        operations->writeAttempts = 0;
        Platform::CpuService cpuService(std::make_unique<NullCpuDataSource>());
        Widgets::WidgetContext context{cpuService};
        UI::WidgetModel model(repository, builtInRegistry(), context);
        QVERIFY(model.loadFromConfig().hasValue());
        QVERIFY(model.setTotalSlots(9));
        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
        QSignalSpy errors(&model, &UI::WidgetModel::persistenceError);

        operations->failWrites = true;
        QVERIFY(!model.setTotalSlots(7));
        QCOMPARE(operations->writeAttempts, 1);
        QCOMPARE(errors.count(), 1);
        QCOMPARE(changed.count(), 0);

        QVERIFY(!model.dropWidget(QStringLiteral("clock"), 4));
        QCOMPARE(operations->writeAttempts, 1);
        QCOMPARE(errors.count(), 1);
        QCOMPARE(changed.count(), 0);
        QVERIFY2(model.lastError().contains("7"), qPrintable(model.lastError()));
        QCOMPARE(model.data(model.index(0), UI::WidgetModel::SlotRole).toInt(), 6);

        operations->failWrites = false;
        QVERIFY(model.setTotalSlots(7));
        QCOMPARE(operations->writeAttempts, 2);
        QCOMPARE(model.data(model.index(0), UI::WidgetModel::SlotRole).toInt(), 4);
        QCOMPARE(changed.count(), 1);

        QVERIFY(model.dropWidget(QStringLiteral("clock"), 4));
        QVERIFY(model.setTotalSlots(10));
        QCOMPARE(operations->writeAttempts, 2);
    }

    void dropSwapsOneCollisionAndRejectsMultipleCollisions()
    {
        QTemporaryDir swapDirectory;
        QVERIFY(swapDirectory.isValid());
        Core::ConfigRepository swapRepository(swapDirectory.filePath("config.json"));
        QVERIFY(swapRepository.save(Core::ConfigDocument{
                                        1,
                                        {
                                            {"clock", "Clock", 0, {}},
                                            {"cpu", "Cpu", 5, {}},
                                        },
                                    })
                    .hasValue());
        Platform::CpuService cpuService(std::make_unique<NullCpuDataSource>());
        Widgets::WidgetContext context{cpuService};
        UI::WidgetModel swapModel(swapRepository, builtInRegistry(), context);
        QVERIFY(swapModel.loadFromConfig().hasValue());
        QVERIFY(swapModel.setTotalSlots(9));
        QSignalSpy swapChanges(&swapModel, &QAbstractItemModel::dataChanged);

        QVERIFY(swapModel.dropWidget(QStringLiteral("clock"), 5));
        QCOMPARE(swapModel.data(swapModel.index(0), UI::WidgetModel::SlotRole).toInt(), 5);
        QCOMPARE(swapModel.data(swapModel.index(1), UI::WidgetModel::SlotRole).toInt(), 0);
        QCOMPARE(swapChanges.count(), 1);
        QCOMPARE(swapChanges.at(0).at(0).value<QModelIndex>().row(), 0);
        QCOMPARE(swapChanges.at(0).at(1).value<QModelIndex>().row(), 1);
        QCOMPARE(swapChanges.at(0).at(2).value<QList<int>>(),
                 QList<int>{UI::WidgetModel::SlotRole});

        QTemporaryDir rejectDirectory;
        QVERIFY(rejectDirectory.isValid());
        Core::ConfigRepository rejectRepository(rejectDirectory.filePath("config.json"));
        QVERIFY(rejectRepository.save(Core::ConfigDocument{
                                          1,
                                          {
                                              {"wide", "Clock", 0, {}},
                                              {"first", "Cpu", 4, {}},
                                              {"second", "Cpu", 6, {}},
                                          },
                                      })
                    .hasValue());
        UI::WidgetModel rejectModel(rejectRepository, builtInRegistry(), context);
        QVERIFY(rejectModel.loadFromConfig().hasValue());
        QVERIFY(rejectModel.setTotalSlots(8));
        const QByteArray before = readBytes(rejectRepository.path());
        QSignalSpy changed(&rejectModel, &QAbstractItemModel::dataChanged);

        QVERIFY(!rejectModel.dropWidget(QStringLiteral("wide"), 4));
        QCOMPARE(changed.count(), 0);
        QCOMPARE(readBytes(rejectRepository.path()), before);
        QCOMPARE(rejectModel.data(rejectModel.index(0), UI::WidgetModel::SlotRole).toInt(), 0);
        QCOMPARE(rejectModel.data(rejectModel.index(1), UI::WidgetModel::SlotRole).toInt(), 4);
        QCOMPARE(rejectModel.data(rejectModel.index(2), UI::WidgetModel::SlotRole).toInt(), 6);
    }

    void saveFailureRollsBackWithoutDataChangedAndReportsError()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath("config.json");
        auto operations = std::make_shared<TrackingOperations>();
        const auto repository = Core::Internal::makeConfigRepository(path, {}, operations);
        QVERIFY(repository.save(Core::ConfigDocument{
                                    1,
                                    {
                                        {"clock", "Clock", 0, {}},
                                        {"cpu", "Cpu", 5, {}},
                                    },
                                })
                    .hasValue());
        operations->writeAttempts = 0;
        Platform::CpuService cpuService(std::make_unique<NullCpuDataSource>());
        Widgets::WidgetContext context{cpuService};
        UI::WidgetModel model(repository, builtInRegistry(), context);
        QVERIFY(model.loadFromConfig().hasValue());
        QVERIFY(model.setTotalSlots(9));
        const QByteArray before = readBytes(path);
        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
        QSignalSpy errors(&model, &UI::WidgetModel::persistenceError);
        operations->failWrites = true;

        QVERIFY(!model.dropWidget(QStringLiteral("clock"), 5));

        QCOMPARE(operations->writeAttempts, 1);
        QCOMPARE(changed.count(), 0);
        QCOMPARE(errors.count(), 1);
        QVERIFY(model.lastError().contains("Injected save failure"));
        QCOMPARE(model.data(model.index(0), UI::WidgetModel::SlotRole).toInt(), 0);
        QCOMPARE(model.data(model.index(1), UI::WidgetModel::SlotRole).toInt(), 5);
        QCOMPARE(readBytes(path), before);
    }

    void noncontiguousChangesEmitSeparateMinimalRanges()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Core::ConfigRepository repository(directory.filePath("config.json"));
        QVERIFY(repository.save(Core::ConfigDocument{
                                    1,
                                    {
                                        {"dragged", "Clock", 0, {}},
                                        {"middle", "Clock", 3, {}},
                                        {"displaced", "Cpu", 6, {}},
                                    },
                                })
                    .hasValue());
        Platform::CpuService cpuService(std::make_unique<NullCpuDataSource>());
        Widgets::WidgetContext context{cpuService};
        UI::WidgetModel model(repository, builtInRegistry(), context);
        QVERIFY(model.loadFromConfig().hasValue());
        QVERIFY(model.setTotalSlots(9));
        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);

        QVERIFY(model.dropWidget(QStringLiteral("dragged"), 6));

        QCOMPARE(changed.count(), 2);
        QCOMPARE(changed.at(0).at(0).value<QModelIndex>().row(), 0);
        QCOMPARE(changed.at(0).at(1).value<QModelIndex>().row(), 0);
        QCOMPARE(changed.at(1).at(0).value<QModelIndex>().row(), 2);
        QCOMPARE(changed.at(1).at(1).value<QModelIndex>().row(), 2);
        QCOMPARE(changed.at(0).at(2).value<QList<int>>(), QList<int>{UI::WidgetModel::SlotRole});
        QCOMPARE(changed.at(1).at(2).value<QList<int>>(), QList<int>{UI::WidgetModel::SlotRole});
    }

    void loadFailureUnknownOnlyAndEmptyDocumentsAreHandled()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath("config.json");
        Core::ConfigRepository repository(path);
        QVERIFY(repository.save(Core::ConfigDocument{
                                    1,
                                    {{"known", "Clock", 0, {}}},
                                })
                    .hasValue());
        Platform::CpuService cpuService(std::make_unique<NullCpuDataSource>());
        Widgets::WidgetContext context{cpuService};
        UI::WidgetModel model(repository, builtInRegistry(), context);
        QVERIFY(model.loadFromConfig().hasValue());
        QObject* const original = viewModelAt(model, 0);
        QVERIFY(original != nullptr);
        QVERIFY(writeBytes(path, R"({"version":1,"widgets":[})"));

        const auto failed = model.loadFromConfig();
        QVERIFY(!failed.hasValue());
        QVERIFY(!failed.error().isEmpty());
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(viewModelAt(model, 0), original);

        QTemporaryDir unknownDirectory;
        QVERIFY(unknownDirectory.isValid());
        Core::ConfigRepository unknownRepository(unknownDirectory.filePath("config.json"));
        const Core::WidgetConfig unknown{
            "future", "FutureWidget", 77, QJsonObject{{"preserve", "yes"}}};
        QVERIFY(unknownRepository.save(Core::ConfigDocument{1, {unknown}}).hasValue());
        UI::WidgetModel unknownModel(unknownRepository, builtInRegistry(), context);
        QVERIFY(unknownModel.loadFromConfig().hasValue());
        QCOMPARE(unknownModel.rowCount(), 0);
        QVERIFY(unknownModel.setTotalSlots(0));
        const auto unknownReloaded = unknownRepository.load();
        QVERIFY(unknownReloaded.hasValue());
        QVERIFY(unknownReloaded.value().widgets.front() == unknown);

        QTemporaryDir emptyDirectory;
        QVERIFY(emptyDirectory.isValid());
        Core::ConfigRepository emptyRepository(emptyDirectory.filePath("config.json"));
        UI::WidgetModel emptyModel(emptyRepository, builtInRegistry(), context);
        QVERIFY(emptyModel.loadFromConfig().hasValue());
        QCOMPARE(emptyModel.rowCount(), 0);
        QVERIFY(emptyModel.setTotalSlots(0));
    }

    void knownFactoryFailureLeavesRepositoryAndExistingModelUntouched()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath("config.json");
        Core::ConfigRepository repository(path);
        QVERIFY(repository.save(Core::ConfigDocument{
                                    1,
                                    {{"stable-id", "Good", 0, {{"keep", "stable"}}}},
                                })
                    .hasValue());
        auto goodDescriptor = Widgets::WidgetDescriptor{
            QStringLiteral("Good"),
            1,
            QUrl(QStringLiteral("qrc:/qt/qml/StatusBar/Good.qml")),
            [](const Core::WidgetConfig& config, Widgets::WidgetContext&)
                -> std::unique_ptr<Widgets::WidgetViewModel> {
                return std::make_unique<NullFactoryViewModel>(config.id);
            },
        };
        auto brokenDescriptor = Widgets::WidgetDescriptor{
            QStringLiteral("Broken"),
            1,
            QUrl(QStringLiteral("qrc:/qt/qml/StatusBar/Broken.qml")),
            [](const Core::WidgetConfig&, Widgets::WidgetContext&)
                -> std::unique_ptr<Widgets::WidgetViewModel> { return nullptr; },
        };
        std::vector<Widgets::WidgetDescriptor> descriptors;
        descriptors.push_back(std::move(goodDescriptor));
        descriptors.push_back(std::move(brokenDescriptor));
        auto registryResult = Widgets::WidgetRegistry::create(std::move(descriptors));
        QVERIFY(registryResult.hasValue());
        Platform::CpuService cpuService(std::make_unique<NullCpuDataSource>());
        Widgets::WidgetContext context{cpuService};
        UI::WidgetModel model(repository, std::move(registryResult).value(), context);
        QVERIFY(model.loadFromConfig().hasValue());
        QObject* const stableViewModel = viewModelAt(model, 0);
        QVERIFY(stableViewModel != nullptr);
        QVERIFY(repository.save(Core::ConfigDocument{
                                    1,
                                    {
                                        {"candidate-id", "Good", 1, {}},
                                        {"broken-id", "Broken", 2, {{"keep", true}}},
                                    },
                                })
                    .hasValue());
        const QByteArray before = readBytes(path);

        const auto loaded = model.loadFromConfig();

        QVERIFY(!loaded.hasValue());
        QVERIFY2(loaded.error().contains("broken-id", Qt::CaseInsensitive),
                 qPrintable(loaded.error()));
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.data(model.index(0), UI::WidgetModel::InstanceIdRole).toString(),
                 QString("stable-id"));
        QCOMPARE(viewModelAt(model, 0), stableViewModel);
        QCOMPARE(readBytes(path), before);
    }
};

QTEST_GUILESS_MAIN(WidgetModelTest)
#include "widget_model_test.moc"
