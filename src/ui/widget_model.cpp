#include "ui/widget_model.h"

#include <algorithm>
#include <exception>
#include <string>
#include <utility>

namespace UI {

WidgetModel::WidgetModel(
    Core::ConfigRepository repository,
    Widgets::WidgetRegistry registry,
    Widgets::WidgetContext& context,
    QObject* parent)
    : QAbstractListModel(parent)
    , m_repository(std::move(repository))
    , m_registry(std::move(registry))
    , m_context(context)
{
}

int WidgetModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_instances.size());
}

QVariant WidgetModel::data(const QModelIndex& modelIndex, const int role) const
{
    if (!modelIndex.isValid() || modelIndex.row() < 0
        || modelIndex.row() >= static_cast<int>(m_instances.size())) {
        return {};
    }

    const auto& instance = m_instances[static_cast<std::size_t>(modelIndex.row())];
    switch (role) {
    case InstanceIdRole:
        return instance.config.id;
    case TypeRole:
        return instance.config.type;
    case SlotRole:
        return instance.config.slot;
    case SpanRole:
        return instance.span;
    case QmlUrlRole:
        return instance.qmlUrl;
    case ViewModelRole:
        return QVariant::fromValue(static_cast<QObject*>(instance.viewModel.get()));
    default:
        return {};
    }
}

QHash<int, QByteArray> WidgetModel::roleNames() const
{
    return {
        {InstanceIdRole, "instanceId"},
        {TypeRole, "type"},
        {SlotRole, "slot"},
        {SpanRole, "span"},
        {QmlUrlRole, "qmlUrl"},
        {ViewModelRole, "viewModel"},
    };
}

Core::Result<void> WidgetModel::loadFromConfig()
{
    auto loaded = m_repository.load();
    if (!loaded.hasValue()) {
        setLastError(loaded.error());
        return Core::Result<void>::failure(loaded.error());
    }

    Core::ConfigDocument loadedDocument = std::move(loaded).value();
    QList<Core::WidgetConfig> unavailableConfigs;
    std::vector<WidgetInstance> loadedInstances;
    loadedInstances.reserve(static_cast<std::size_t>(loadedDocument.widgets.size()));

    for (const auto& config : loadedDocument.widgets) {
        const auto* const descriptor = m_registry.find(config.type);
        if (descriptor == nullptr) {
            unavailableConfigs.append(config);
            continue;
        }

        std::unique_ptr<Widgets::WidgetViewModel> viewModel;
        try {
            viewModel = descriptor->create(config, m_context);
        } catch (const std::exception& error) {
            const QString message = QStringLiteral(
                "Cannot create widget '%1' of type '%2': %3")
                                        .arg(config.id, config.type)
                                        .arg(QString::fromUtf8(error.what()));
            setLastError(message);
            return Core::Result<void>::failure(message);
        } catch (...) {
            const QString message = QStringLiteral(
                "Cannot create widget '%1' of type '%2': unknown factory error")
                                        .arg(config.id, config.type);
            setLastError(message);
            return Core::Result<void>::failure(message);
        }

        if (viewModel == nullptr) {
            const QString message = QStringLiteral(
                "Cannot create widget '%1' of type '%2': factory returned null")
                                        .arg(config.id, config.type);
            setLastError(message);
            return Core::Result<void>::failure(message);
        }
        if (viewModel->instanceId() != config.id) {
            const QString message = QStringLiteral(
                "Cannot create widget '%1' of type '%2': factory changed instance identity")
                                        .arg(config.id, config.type);
            setLastError(message);
            return Core::Result<void>::failure(message);
        }
        viewModel->setParent(nullptr);
        loadedInstances.push_back(
            WidgetInstance{config, descriptor->span, descriptor->qmlUrl, std::move(viewModel)});
    }

    beginResetModel();
    m_document = std::move(loadedDocument);
    m_unavailableConfigs = std::move(unavailableConfigs);
    m_instances = std::move(loadedInstances);
    m_requestedTotalSlots = -1;
    m_layoutReadyForRequestedSlots = false;
    endResetModel();
    setLastError({});
    return Core::Result<void>::success();
}

bool WidgetModel::setTotalSlots(const int totalSlots)
{
    m_requestedTotalSlots = totalSlots;
    m_layoutReadyForRequestedSlots = false;

    if (totalSlots < 0) {
        setLastError(QStringLiteral("Cannot normalize widget layout: total slots is negative"));
        return false;
    }

    const auto current = currentLayout();
    const auto normalized = Core::LayoutEngine::normalize(current, totalSlots);
    if ((normalized.empty() && !current.empty())
        || !Core::LayoutEngine::isValid(normalized, totalSlots)) {
        setLastError(QStringLiteral(
            "Cannot normalize widget layout: widgets do not fit in %1 slots")
                         .arg(totalSlots));
        return false;
    }
    return commitLayout(normalized, totalSlots);
}

bool WidgetModel::dropWidget(const QString& instanceId, const int targetSlot)
{
    const auto current = currentLayout();
    if (!m_layoutReadyForRequestedSlots || m_requestedTotalSlots < 0
        || !Core::LayoutEngine::isValid(current, m_requestedTotalSlots)) {
        setLastError(QStringLiteral(
            "Cannot move widget: layout is not ready for requested %1 slots")
                         .arg(m_requestedTotalSlots));
        return false;
    }

    const auto candidate = Core::LayoutEngine::drop(
        current, instanceId.toStdString(), targetSlot, m_requestedTotalSlots);
    if (!candidate.has_value()) {
        setLastError(QStringLiteral(
            "Cannot move widget '%1': layout transaction was rejected")
                         .arg(instanceId));
        return false;
    }
    return commitLayout(*candidate, m_requestedTotalSlots);
}

std::vector<Core::LayoutItem> WidgetModel::currentLayout() const
{
    std::vector<Core::LayoutItem> layout;
    layout.reserve(m_instances.size());
    for (const auto& instance : m_instances) {
        layout.push_back({
            instance.config.id.toStdString(),
            instance.config.slot,
            instance.span,
        });
    }
    return layout;
}

Core::Result<Core::ConfigDocument> WidgetModel::documentWithLayout(
    const std::vector<Core::LayoutItem>& layout) const
{
    if (layout.size() != m_instances.size()) {
        return Core::Result<Core::ConfigDocument>::failure(
            QStringLiteral("Cannot save widget layout: candidate size changed"));
    }

    Core::ConfigDocument document = m_document;
    for (std::size_t index = 0; index < layout.size(); ++index) {
        const auto& instance = m_instances[index];
        const auto& item = layout[index];
        if (item.id != instance.config.id.toStdString() || item.span != instance.span) {
            return Core::Result<Core::ConfigDocument>::failure(
                QStringLiteral("Cannot save widget layout: candidate identity changed"));
        }

        const auto match = std::find_if(
            document.widgets.begin(),
            document.widgets.end(),
            [&instance](const Core::WidgetConfig& config) {
                return config.id == instance.config.id;
            });
        if (match == document.widgets.end()) {
            return Core::Result<Core::ConfigDocument>::failure(
                QStringLiteral("Cannot save widget layout: config id '%1' is missing")
                    .arg(instance.config.id));
        }
        match->slot = item.slot;
    }
    return Core::Result<Core::ConfigDocument>::success(std::move(document));
}

bool WidgetModel::commitLayout(
    const std::vector<Core::LayoutItem>& layout,
    const int totalSlots)
{
    const auto current = currentLayout();
    if (layout == current) {
        m_layoutReadyForRequestedSlots = true;
        setLastError({});
        return true;
    }
    if (!Core::LayoutEngine::isValid(layout, totalSlots)) {
        setLastError(QStringLiteral("Cannot save widget layout: candidate is invalid"));
        return false;
    }

    auto updatedDocument = documentWithLayout(layout);
    if (!updatedDocument.hasValue()) {
        setLastError(updatedDocument.error());
        return false;
    }

    const auto saved = m_repository.save(updatedDocument.value());
    if (!saved.hasValue()) {
        setLastError(QStringLiteral("Cannot save widget layout: %1").arg(saved.error()));
        emit persistenceError(m_lastError);
        return false;
    }

    std::vector<int> changedRows;
    for (std::size_t index = 0; index < m_instances.size(); ++index) {
        if (m_instances[index].config.slot != layout[index].slot) {
            changedRows.push_back(static_cast<int>(index));
        }
    }

    m_document = std::move(updatedDocument).value();
    for (const int row : changedRows) {
        m_instances[static_cast<std::size_t>(row)].config.slot =
            layout[static_cast<std::size_t>(row)].slot;
    }
    m_layoutReadyForRequestedSlots = true;
    emitSlotChanges(changedRows);
    setLastError({});
    return true;
}

void WidgetModel::emitSlotChanges(const std::vector<int>& changedRows)
{
    if (changedRows.empty()) {
        return;
    }

    int rangeStart = changedRows.front();
    int rangeEnd = rangeStart;
    for (std::size_t index = 1; index < changedRows.size(); ++index) {
        const int row = changedRows[index];
        if (row == rangeEnd + 1) {
            rangeEnd = row;
            continue;
        }
        emit dataChanged(this->index(rangeStart), this->index(rangeEnd), {SlotRole});
        rangeStart = row;
        rangeEnd = row;
    }
    emit dataChanged(this->index(rangeStart), this->index(rangeEnd), {SlotRole});
}

void WidgetModel::setLastError(QString error)
{
    if (m_lastError == error) {
        return;
    }
    m_lastError = std::move(error);
    emit lastErrorChanged();
}

} // namespace UI
