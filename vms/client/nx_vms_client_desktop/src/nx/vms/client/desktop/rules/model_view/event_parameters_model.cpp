// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_parameters_model.h"

#include <nx/vms/client/desktop/rules/utils/separator.h>

namespace {

const QString kSeparatorSymbol = "-";

} // namespace

namespace nx::vms::client::desktop::rules {

using namespace nx::vms::rules::utils;
struct EventParametersModel::Private
{
    QList<QString> visibleElements;

    using EventParametersByGroup = std::map<QString, QSet<QString>>;
    EventParametersByGroup subGroupToElements;
    EventParametersByGroup defaultEventParametersByGroup;
    QList<QString> defaultVisibleElements;
    const QSet<QString> allElements;
    QString currentExpandedGroup;

    void setVisibleElements(const EventParametersByGroup& eventParameterByGroup)
    {
        visibleElements.clear();
        for (auto& [group, names]: eventParameterByGroup)
        {
            for (auto& val: names)
                visibleElements.push_back(EventParameterHelper::addBrackets(val));

            visibleElements.push_back(kSeparatorSymbol);
        }

        if (!visibleElements.empty() && visibleElements.back() == kSeparatorSymbol)
            visibleElements.removeLast();
    }

    void groupEventParameters()
    {
        defaultEventParametersByGroup.clear();
        for (auto& parameter: allElements)
        {
            auto mainGroupName = EventParameterHelper::getMainGroupName(parameter);
            if (EventParameterHelper::containsSubgroups(parameter))
            {
                // If parameter consists of more than two parts, must be displayed only first
                // two (e.g. {event.attribute} )
                const QString subGroupName = EventParameterHelper::getSubGroupName(parameter);
                // If the user selects such parameter from the list or type it manually,
                // the list must display the next level (e.g. [{event.attribute.Number}]).
                subGroupToElements[subGroupName].insert(parameter);
                defaultEventParametersByGroup[mainGroupName].insert(subGroupName);
            }
            else
            {
                defaultEventParametersByGroup[mainGroupName].insert(parameter);
            }
        }
        setVisibleElements(defaultEventParametersByGroup);
    }

    bool isSeparator(const QModelIndex& index) const
    {
        return isSeparator(visibleElements[index.row()]);
    }

    bool isSeparator(const QString& value) const
    {
        return value == kSeparatorSymbol;
    }
};

EventParametersModel::EventParametersModel(
    const EventParameterHelper::EventParametersNames& eventParameters, QObject* parent):
    QAbstractListModel{parent},
    d(new Private{.allElements = eventParameters })
{
    d->groupEventParameters();
    d->defaultVisibleElements = d->visibleElements;
}

EventParametersModel::~EventParametersModel()
{
}

int EventParametersModel::rowCount(const QModelIndex& /*parent*/) const
{
    return d->visibleElements.size();
}

QVariant EventParametersModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= d->visibleElements.size())
        return {};

    const bool sep = d->isSeparator(index);
    if (role == Qt::AccessibleDescriptionRole && sep)
        return QVariant::fromValue(Separator{});

    // EditRole is required for the Completer.
    if ((role == Qt::DisplayRole || role == Qt::EditRole) && !sep)
        return d->visibleElements[index.row()];

    return {};
}

bool EventParametersModel::isSubGroupName(const QString& groupName)
{
    if (groupName.isEmpty())
        return false;

    return d->subGroupToElements.contains(EventParameterHelper::removeBrackets(groupName));
}

void EventParametersModel::expandGroup(const QString& groupNameToExpand)
{
    if (groupNameToExpand.isEmpty())
        return;

    const auto subGroupName = EventParameterHelper::getSubGroupName(groupNameToExpand);

    if (subGroupName == d->currentExpandedGroup)
    {
        // This subgroup was already expanded. No action required.
        return;
    }

    const auto groupElements = d->subGroupToElements.find(subGroupName);
    if (groupElements == d->subGroupToElements.end())
    {
        // Value not part of subgroup either subgroup, reseting model to default.
        resetExpandedGroup();
        return;
    }

    const QString parentGroup = EventParameterHelper::getMainGroupName(subGroupName);
    auto expandedModel = d->defaultEventParametersByGroup;
    for (auto& el: groupElements->second)
        expandedModel[parentGroup].insert(el);

    beginResetModel();

    d->setVisibleElements(expandedModel);
    if (!isCorrectParameter(subGroupName))
        d->visibleElements.removeOne(EventParameterHelper::addBrackets(subGroupName));

    d->currentExpandedGroup = subGroupName;

    endResetModel();
}

void EventParametersModel::resetExpandedGroup()
{
    setDefaultVisibleElements();
    d->currentExpandedGroup.clear();
}

bool EventParametersModel::isCorrectParameter(const QString& eventParameter)
{
    return d->allElements.contains(eventParameter);
}

void EventParametersModel::setDefaultVisibleElements()
{
    beginResetModel();
    d->visibleElements = d->defaultVisibleElements;
    endResetModel();
}

void EventParametersModel::filter(const QString& filter)
{
    if (filter.size() == 1 && EventParameterHelper::isStartOfEventParameter(filter.front()))
    {
        setDefaultVisibleElements();
        return; //< It's begining of event parameter, no filter is required.
    }

    if (d->visibleElements.empty() || filter.isEmpty())
        return;


    if (!EventParameterHelper::isStartOfEventParameter(filter.front()))
    {
        // Filter is not correct event parameter. Set visible elements to empty value.
        beginResetModel();
        d->visibleElements.clear();
        endResetModel();
        return;
    }

    for (int i = d->visibleElements.size() - 1; i >= 0; --i)
    {
        const auto& value = d->visibleElements[i];
        const bool shouldKeepSeparator = i > 0 && i < d->visibleElements.size() - 1
            && !d->isSeparator(d->visibleElements[i + 1]);

        if (d->isSeparator(value) && shouldKeepSeparator)
            continue; //< Keep separator for grouping of the element.

        if (!value.startsWith(filter))
        {
            beginRemoveRows({}, i, i);
            d->visibleElements.erase(d->visibleElements.begin() + i);
            endRemoveRows();
        }
    }

    // Remove separator on first line, if present.
    if (!d->visibleElements.empty() && d->isSeparator(d->visibleElements.first()))
    {
        beginRemoveRows({}, 0, 0);
        d->visibleElements.removeFirst();
        endRemoveRows();
    }
}

} // namespace nx::vms::client::desktop::rules
