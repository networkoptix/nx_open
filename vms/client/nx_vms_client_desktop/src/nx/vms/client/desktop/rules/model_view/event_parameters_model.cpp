// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_parameters_model.h"

namespace {

const QString kSeparatorSymbol = "-";

} // namespace

namespace nx::vms::client::desktop::rules {

bool EventParametersModel::isSeparator(const QModelIndex& index) const
{
    return groupedEventParametersNames[index.row()] == kSeparatorSymbol;
}

EventParametersModel::EventParametersModel(
    const nx::vms::rules::TextWithFields::EventParametersByGroup& eventParameters,
    QObject* parent)
    :
    QAbstractListModel{parent}
{
    for (auto& [group, names]: eventParameters)
    {
        for (auto& val: names)
            groupedEventParametersNames.push_back(val);
        groupedEventParametersNames.push_back(kSeparatorSymbol);
    }

    if (!groupedEventParametersNames.empty()
        && groupedEventParametersNames.back() == kSeparatorSymbol)
    {
        groupedEventParametersNames.removeLast();
    }
}

int EventParametersModel::rowCount(const QModelIndex& parent) const
{
    return groupedEventParametersNames.size();
}

QVariant EventParametersModel::data(const QModelIndex& index, int role) const
{
    if (role == Qt::AccessibleDescriptionRole && isSeparator(index))
        return "separator";

    // EditRole is required for the Completer.
    if ((role == Qt::DisplayRole || role == Qt::EditRole) && !isSeparator(index))
        return groupedEventParametersNames[index.row()];

    return {};
}

} // namespace nx::vms::client::desktop::rules
