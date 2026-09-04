#pragma once
#include "core/config_repository.h"
#include "core/layout_engine.h"
#include "core/widget_config.h"
#include "platform/cpu_service.h"
#include "widgets/i_widget_view_model.h"

#include <QAbstractListModel>
#include <QJsonObject>
#include <QString>
#include <memory>
#include <vector>

namespace UI {
    struct WidgetInstance {
        std::unique_ptr<Widgets::IWidgetViewModel> vm;
        int slot;
        QString id;
        QJsonObject settings;
    };

    class WidgetModel : public QAbstractListModel {
        Q_OBJECT
        Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    public:
        enum WidgetRoles {
            KindRole = Qt::UserRole + 1,
            SlotRole,
            SpanRole,
            ViewModelRole
        };

        explicit WidgetModel(Core::ConfigRepository repository, QObject* parent = nullptr);
        
        int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
        QHash<int, QByteArray> roleNames() const override;

        [[nodiscard]] Core::Result<void> loadFromConfig();
        [[nodiscard]] const QString& lastError() const noexcept;
        void updateAll();

        // 动态接收 QML 实时比例算出的尺寸，实现无损吸附碰撞
        Q_INVOKABLE bool handleWidgetDropped(int draggedIndex, float dropCenterX, float cellWidth, float spacing, float containerWidth);

    signals:
        void lastErrorChanged();
        void persistenceError(const QString& message);

    private:
        [[nodiscard]] Core::Result<Core::ConfigDocument> documentWithLayout(
            const std::vector<Core::LayoutItem>& layout) const;
        void setLastError(QString error);

        Core::ConfigRepository m_repository;
        Core::ConfigDocument m_document;
        std::unique_ptr<Platform::CpuService> m_cpuService;
        std::vector<WidgetInstance> m_instances;
        QString m_lastError;
    };
}
