#pragma once

#include "core/result.h"
#include "widgets/widget_descriptor.h"

#include <QString>

#include <cstddef>
#include <utility>
#include <vector>

namespace Widgets {

class WidgetRegistry final {
public:
    [[nodiscard]] static Core::Result<WidgetRegistry> create(
        std::vector<WidgetDescriptor> descriptors);

    [[nodiscard]] const WidgetDescriptor* find(const QString& type) const noexcept;
    [[nodiscard]] const std::vector<WidgetDescriptor>& descriptors() const noexcept
    {
        return m_descriptors;
    }

private:
    explicit WidgetRegistry(std::vector<WidgetDescriptor> descriptors)
        : m_descriptors(std::move(descriptors))
    {
    }

    std::vector<WidgetDescriptor> m_descriptors;
};

inline Core::Result<WidgetRegistry> WidgetRegistry::create(
    std::vector<WidgetDescriptor> descriptors)
{
    for (std::size_t index = 0; index < descriptors.size(); ++index) {
        const auto& descriptor = descriptors[index];
        if (descriptor.type.trimmed().isEmpty()) {
            return Core::Result<WidgetRegistry>::failure(
                QStringLiteral("Widget descriptor %1 type must be nonempty").arg(index));
        }
        if (descriptor.span <= 0) {
            return Core::Result<WidgetRegistry>::failure(
                QStringLiteral("Widget descriptor '%1' span must be positive")
                    .arg(descriptor.type));
        }
        if (descriptor.qmlUrl.isEmpty() || !descriptor.qmlUrl.isValid()
            || descriptor.qmlUrl.isRelative()
            || descriptor.qmlUrl.scheme().compare(
                   QStringLiteral("qrc"), Qt::CaseInsensitive)
                != 0
            || !descriptor.qmlUrl.authority().isEmpty()
            || !descriptor.qmlUrl.path().startsWith(QLatin1Char('/'))
            || descriptor.qmlUrl.path() == QLatin1String("/")) {
            return Core::Result<WidgetRegistry>::failure(
                QStringLiteral(
                    "Widget descriptor '%1' URL must be a valid absolute qrc URL")
                    .arg(descriptor.type));
        }
        if (!descriptor.create) {
            return Core::Result<WidgetRegistry>::failure(
                QStringLiteral("Widget descriptor '%1' factory must be present")
                    .arg(descriptor.type));
        }

        for (std::size_t previous = 0; previous < index; ++previous) {
            if (descriptors[previous].type == descriptor.type) {
                return Core::Result<WidgetRegistry>::failure(
                    QStringLiteral("Widget descriptor type '%1' is duplicated")
                        .arg(descriptor.type));
            }
        }
    }

    return Core::Result<WidgetRegistry>::success(
        WidgetRegistry(std::move(descriptors)));
}

inline const WidgetDescriptor* WidgetRegistry::find(const QString& type) const noexcept
{
    for (const auto& descriptor : m_descriptors) {
        if (descriptor.type == type) {
            return &descriptor;
        }
    }
    return nullptr;
}

} // namespace Widgets
