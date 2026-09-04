#include "ui/widget_model.h"
#include "widgets/widget_registry.h"
#include <cmath>
#include <algorithm>
#include <utility>

namespace UI {
    WidgetModel::WidgetModel(Core::ConfigRepository repository, QObject* parent)
        : QAbstractListModel(parent)
        , m_repository(std::move(repository))
    {
    }

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

    Core::Result<void> WidgetModel::loadFromConfig() {
        auto config = m_repository.load();
        if (!config.hasValue()) {
            setLastError(config.error());
            return Core::Result<void>::failure(config.error());
        }

        Core::ConfigDocument loadedDocument = std::move(config).value();
        std::vector<WidgetInstance> loadedInstances;
        const auto& registry = Widgets::WidgetRegistry::GetInstance();

        for (const auto& wConfig : loadedDocument.widgets) {
            if (auto widget = registry.Create(wConfig.type.toStdString())) {
                loadedInstances.push_back(
                    WidgetInstance{std::move(widget), wConfig.slot, wConfig.id, wConfig.settings});
            }
        }

        beginResetModel();
        m_document = std::move(loadedDocument);
        m_instances = std::move(loadedInstances);
        endResetModel();
        setLastError({});
        return Core::Result<void>::success();
    }

    const QString& WidgetModel::lastError() const noexcept {
        return m_lastError;
    }

    void WidgetModel::updateAll() {
        for (auto& instance : m_instances) {
            instance.vm->Update();
        }
    }

    bool WidgetModel::handleWidgetDropped(int draggedIndex, float dropCenterX, float cellWidth, float spacing, float containerWidth) {
        if (draggedIndex < 0 || draggedIndex >= static_cast<int>(m_instances.size())) {
            setLastError(QStringLiteral("Cannot move widget: index %1 is out of range")
                             .arg(draggedIndex));
            return false;
        }

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
        if (oldSlot == targetSlot) {
            setLastError({});
            return true;
        }

        std::vector<int> previousSlots;
        previousSlots.reserve(m_instances.size());
        for (const auto& instance : m_instances) {
            previousSlots.push_back(instance.slot);
        }

        for (size_t i = 0; i < m_instances.size(); ++i) {
            if (i == static_cast<size_t>(draggedIndex)) continue;
            int otherStart = m_instances[i].slot;
            int otherEnd = otherStart + m_instances[i].vm->GetSpan() - 1;
            int targetEnd = targetSlot + span - 1;

            if (std::max(targetSlot, otherStart) <= std::min(targetEnd, otherEnd)) {
                m_instances[i].slot = oldSlot;
            }
        }

        m_instances[draggedIndex].slot = targetSlot;

        auto updatedDocument = documentWithCurrentSlots();
        if (!updatedDocument.hasValue()) {
            for (size_t i = 0; i < m_instances.size(); ++i) {
                m_instances[i].slot = previousSlots[i];
            }
            setLastError(updatedDocument.error());
            return false;
        }

        const auto saveResult = m_repository.save(updatedDocument.value());
        if (!saveResult.hasValue()) {
            for (size_t i = 0; i < m_instances.size(); ++i) {
                m_instances[i].slot = previousSlots[i];
            }
            setLastError(QStringLiteral("Cannot save widget layout: %1").arg(saveResult.error()));
            return false;
        }

        m_document = std::move(updatedDocument).value();
        for (size_t i = 0; i < m_instances.size(); ++i) {
            if (m_instances[i].slot != previousSlots[i]) {
                const QModelIndex changed = index(static_cast<int>(i));
                emit dataChanged(changed, changed, {SlotRole});
            }
        }
        setLastError({});
        return true;
    }

    Core::Result<Core::ConfigDocument> WidgetModel::documentWithCurrentSlots() const {
        Core::ConfigDocument document = m_document;
        for (const auto& instance : m_instances) {
            const auto match = std::find_if(
                document.widgets.begin(),
                document.widgets.end(),
                [&instance](const Core::WidgetConfig& config) {
                    return config.id == instance.id;
                });
            if (match == document.widgets.end()) {
                return Core::Result<Core::ConfigDocument>::failure(
                    QStringLiteral("Cannot save widget layout: config id '%1' is missing")
                        .arg(instance.id));
            }
            match->slot = instance.slot;
        }
        return Core::Result<Core::ConfigDocument>::success(std::move(document));
    }

    void WidgetModel::setLastError(QString error) {
        if (m_lastError == error) {
            return;
        }
        m_lastError = std::move(error);
        emit lastErrorChanged();
    }
}
