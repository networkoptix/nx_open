// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_attributed_event_model.h"

#include <nx/utils/string.h>
#include <nx/vms/client/core/client_core_globals.h>
#include <nx/vms/client/core/skin/color_theme.h>

namespace nx::vms::client::core {

QVariant AbstractAttributedEventModel::data(const QModelIndex& index, int role) const
{
    switch (role)
    {
        case FlatAttributeListRole:
        {
            const auto analyticsAttributes = data(index, AnalyticsAttributesRole)
                .value<analytics::AttributeList>();

            return flattenAttributeList(analyticsAttributes);
        }

        default:
            return base_type::data(index, role);
    }
}

QVariantList AbstractAttributedEventModel::flattenAttributeList(
    const analytics::AttributeList& source)
{
    QVariantList result;

    for (const auto& attribute: source)
    {
        result.append(QJsonObject{{"text", attribute.displayedName}});
        result.append(QJsonObject{
            {"text", valuesText(attribute.displayedValues)},
            {"colors", QJsonValue::fromVariant(attribute.colorValues)},
            });
    }

    return result;
}

QString AbstractAttributedEventModel::valuesText(const QStringList& values)
{
    // TODO: #vkutin Move this logic to the view side.
    static constexpr int kMaxDisplayedGroupValues = 2; //< Before "(+n values)".

    const int totalCount = values.size();
    const QString displayedValues = nx::utils::strJoin(values.cbegin(),
        values.cbegin() + std::min(totalCount, kMaxDisplayedGroupValues),
        ", ");

    const int valuesLeft = totalCount - kMaxDisplayedGroupValues;
    if (valuesLeft <= 0)
        return displayedValues;

    return nx::format("%1 <font color=\"%3\">(%2)</font>", displayedValues,
        tr("+%n values", "", valuesLeft),
        colorTheme()->color("light16").name());
}

} // namespace nx::vms::client::core
