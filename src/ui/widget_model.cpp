#include "ui/widget_model.h"
#include "core/config_manager.h"
#include "widgets/widget_registry.h"
#include <cmath>
#include <algorithm>

namespace UI {
    WidgetModel::WidgetModel(QObject* parent) : QAbstractListModel(parent) {}

    int WidgetModel::rowCount(const QModelIndex& parent) const {
        if (parent.isValid()) return 0;
        return static_cast<int>(m_instances.size());
    }

    QVariant WidgetModel::data(const QModelIndex& index, int role) const {
        if (!index.isValid() || index.row() >= static_cast<int>(m_instances.size())) return {};
        
        const auto& instance = m_instances[index.row()];
        if (role == KindRole) return QString::fromStdString(instance.vm->GetKind());
        if (role == SlotRole) return instance.slot;
        if (role == SpanRole) return instance.vm->GetSpan();
        
        return {};
    }

    QHash<int, QByteArray> WidgetModel::roleNames() const {
        return {
            {KindRole, "kind"},
            {SlotRole, "slot"},
            {SpanRole, "span"}
        };
    }

    void WidgetModel::loadFromConfig() {
        beginResetModel();
        m_instances.clear();
        
        auto& configMgr = Core::ConfigManager::GetInstance();
        configMgr.Load();
        auto activeWidgets = configMgr.GetActiveWidgets();
        const auto& registry = Widgets::WidgetRegistry::GetInstance();

        for (const auto& wConfig : activeWidgets) {
            if (auto widget = registry.Create(wConfig.name)) {
                m_instances.push_back(WidgetInstance{std::move(widget), wConfig.slot});
            }
        }
        endResetModel();
        refreshLayoutAndSync();
    }

    void WidgetModel::updateAll() {
        for (auto& instance : m_instances) {
            instance.vm->Update();
        }
    }

    void WidgetModel::handleWidgetDropped(int draggedIndex, float dropCenterX, float cellWidth, float spacing, float containerWidth) {
        if (draggedIndex < 0 || draggedIndex >= static_cast<int>(m_instances.size())) return;

        float unitWidth = cellWidth + spacing;
        int span = m_instances[draggedIndex].vm->GetSpan();
        float widgetWidth = (span * cellWidth) + std::max(0, span - 1) * spacing;
        
        float dropLeftX = dropCenterX - (widgetWidth / 2.0f);
        int targetSlot = static_cast<int>(std::round(dropLeftX / unitWidth));

        if (targetSlot < 0) targetSlot = 0;

        if (containerWidth > 0.0f) {
            int rawSlots = static_cast<int>(std::round((containerWidth + spacing) / unitWidth));
            int totalSlots = (rawSlots % 2 == 1) ? rawSlots : std::max(1, rawSlots - 1);
            int maxSlot = std::max(0, totalSlots - span);
            
            if (targetSlot > maxSlot) {
                targetSlot = maxSlot;
            }
        }

        int oldSlot = m_instances[draggedIndex].slot;

        for (size_t i = 0; i < m_instances.size(); ++i) {
            if (i == static_cast<size_t>(draggedIndex)) continue;
            int otherStart = m_instances[i].slot;
            int otherEnd = otherStart + m_instances[i].vm->GetSpan() - 1;
            int targetEnd = targetSlot + span - 1;

            if (std::max(targetSlot, otherStart) <= std::min(targetEnd, otherEnd)) {
                m_instances[i].slot = oldSlot;
                QModelIndex idx = index(static_cast<int>(i));
                emit dataChanged(idx, idx, {SlotRole});
            }
        }

        m_instances[draggedIndex].slot = targetSlot;
        
        QModelIndex idx = index(draggedIndex);
        emit dataChanged(idx, idx, {SlotRole});

        if (oldSlot != targetSlot) {
            refreshLayoutAndSync();
        }
    }

    void WidgetModel::refreshLayoutAndSync() {
        std::vector<Core::WidgetConfig> newConfigList;
        newConfigList.reserve(m_instances.size());
        for (const auto& instance : m_instances) {
            newConfigList.push_back({instance.vm->GetKind(), instance.slot});
        }
        Core::ConfigManager::GetInstance().SetActiveWidgets(newConfigList);
    }
}
