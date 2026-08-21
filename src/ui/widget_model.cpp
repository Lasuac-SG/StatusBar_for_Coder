#include "src/ui/widget_model.h"
#include "src/ui/viewmodels/widget_registry.h"
#include <cmath>
#include <algorithm>

namespace UI {
    WidgetModel::WidgetModel(QObject* parent) : QAbstractListModel(parent), m_maxSlots(0) {}

    int WidgetModel::rowCount(const QModelIndex& parent) const {
        if (parent.isValid()) return 0;
        return static_cast<int>(m_instances.size());
    }

    QVariant WidgetModel::data(const QModelIndex& index, int role) const {
        if (!index.isValid() || index.row() >= m_instances.size()) return QVariant();
        
        const auto& instance = m_instances[index.row()];
        if (role == KindRole) return QString::fromStdString(instance.vm->GetName());
        if (role == SlotRole) return instance.slot;
        if (role == SpanRole) return instance.vm->GetSpan();
        
        return QVariant();
    }

    QHash<int, QByteArray> WidgetModel::roleNames() const {
        QHash<int, QByteArray> roles;
        roles[KindRole] = "kind";
        roles[SlotRole] = "slot";
        roles[SpanRole] = "span";
        return roles;
    }

    void WidgetModel::loadFromConfig() {
        beginResetModel();
        m_instances.clear();
        
        auto& configMgr = Core::ConfigManager::GetInstance();
        configMgr.Load();
        auto activeWidgets = configMgr.GetActiveWidgets();
        auto& registry = WidgetRegistry::GetInstance();

        for (const auto& wConfig : activeWidgets) {
            // 修复：移除多余参数，并显式构造
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

    void UI::WidgetModel::handleWidgetDropped(int draggedIndex, float dropCenterX, float dpiScale, float containerWidth) {
        if (draggedIndex < 0 || draggedIndex >= static_cast<int>(m_instances.size())) return;

        float cellWidth = 40.0f * dpiScale;
        float spacing = 8.0f * dpiScale;
        float unitWidth = cellWidth + spacing;
        int span = m_instances[draggedIndex].vm->GetSpan();
        float widgetWidth = (span * cellWidth) + std::max(0, span - 1) * spacing;
        
        float dropLeftX = dropCenterX - (widgetWidth / 2.0f);
        int targetSlot = static_cast<int>(std::round(dropLeftX / unitWidth));

        // 1. 左边界限制
        if (targetSlot < 0) targetSlot = 0;

        // 2. 右边界限制：基于奇数网格总槽位计算
        if (containerWidth > 0.0f) {
            int rawSlots = static_cast<int>(std::round((containerWidth + spacing) / unitWidth));
            int totalSlots = (rawSlots % 2 == 1) ? rawSlots : std::max(1, rawSlots - 1);
            int maxSlot = std::max(0, totalSlots - span);
            
            if (targetSlot > maxSlot) {
                targetSlot = maxSlot;
            }
        }

        int oldSlot = m_instances[draggedIndex].slot;

        // 碰撞检测与槽位交换
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
        for (const auto& instance : m_instances) {
            newConfigList.push_back({instance.vm->GetName(), instance.slot});
        }
        Core::ConfigManager::GetInstance().SetActiveWidgets(newConfigList);
    }
}
