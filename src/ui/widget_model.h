#pragma once
#include <QAbstractListModel>
#include <vector>
#include "src/core/config_manager.h"
#include "src/ui/viewmodels/i_view_model.h"
#include <memory>

namespace UI {
    struct WidgetInstance {
        std::unique_ptr<IViewModel> vm;
        int slot;
    };

    // 继承 QAbstractListModel 以便 QML 可以直接使用 ListView/Repeater 渲染
    class WidgetModel : public QAbstractListModel {
        Q_OBJECT
    public:
        enum WidgetRoles {
            KindRole = Qt::UserRole + 1,
            SlotRole,
            SpanRole
        };

        WidgetModel(QObject* parent = nullptr);
        
        int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
        QHash<int, QByteArray> roleNames() const override;

        void loadFromConfig();
        void updateAll();

        // 供 QML 在拖拽松手时调用的方法
        Q_INVOKABLE void handleWidgetDropped(int draggedIndex, float dropCenterX, float dpiScale, float containerWidth);

    private:
        void refreshLayoutAndSync();
        std::vector<WidgetInstance> m_instances;
        int m_maxSlots;
    };
}
