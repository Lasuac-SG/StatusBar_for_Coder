#pragma once

#include "core/config_repository.h"
#include "core/layout_engine.h"
#include "core/widget_config.h"
#include "widgets/widget_descriptor.h"
#include "widgets/widget_registry.h"

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QUrl>

#include <memory>
#include <vector>

namespace UI {

class WidgetModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    enum WidgetRoles {
        InstanceIdRole = Qt::UserRole + 1,
        TypeRole,
        SlotRole,
        SpanRole,
        QmlUrlRole,
        ViewModelRole,
    };

    WidgetModel(
        Core::ConfigRepository repository,
        Widgets::WidgetRegistry registry,
        Widgets::WidgetContext& context,
        QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(
        const QModelIndex& index,
        int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] Core::Result<void> loadFromConfig();
    [[nodiscard]] const QString& lastError() const noexcept { return m_lastError; }

    Q_INVOKABLE bool setTotalSlots(int totalSlots);
    Q_INVOKABLE bool dropWidget(const QString& instanceId, int targetSlot);

signals:
    void lastErrorChanged();
    void persistenceError(const QString& message);

private:
    struct WidgetInstance final {
        Core::WidgetConfig config;
        int span{};
        QUrl qmlUrl;
        std::unique_ptr<Widgets::WidgetViewModel> viewModel;
    };

    [[nodiscard]] std::vector<Core::LayoutItem> currentLayout() const;
    [[nodiscard]] Core::Result<Core::ConfigDocument> documentWithLayout(
        const std::vector<Core::LayoutItem>& layout) const;
    [[nodiscard]] bool commitLayout(
        const std::vector<Core::LayoutItem>& layout,
        int totalSlots);
    void emitSlotChanges(const std::vector<int>& changedRows);
    void setLastError(QString error);

    Core::ConfigRepository m_repository;
    Widgets::WidgetRegistry m_registry;
    Widgets::WidgetContext m_context;
    Core::ConfigDocument m_document;
    QList<Core::WidgetConfig> m_unavailableConfigs;
    std::vector<WidgetInstance> m_instances;
    int m_requestedTotalSlots{-1};
    bool m_layoutReadyForRequestedSlots{};
    QString m_lastError;
};

} // namespace UI
