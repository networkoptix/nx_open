// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "push_systems_selection_model.h"

#include <QtQml/QtQml>

#include <nx/utils/math/math.h>
#include <nx/utils/qt_helpers.h>

#include "push_notification_structures.h"

namespace nx::vms::client::mobile::details {

namespace {

enum Roles
{
    SystemIdRole = Qt::UserRole
};

QnCloudSystemList sortedByName(QnCloudSystemList value)
{
    std::sort(value.begin(), value.end(),
        [](const QnCloudSystem& left, const QnCloudSystem& right)
        {
            return left.name == right.name
                ? left.cloudId < right.cloudId
                : left.name < right.name;
        });
    return value;
}

SystemSet operator-(const SystemSet& lhs, const SystemSet& rhs)
{
    SystemSet result;
    std::set_difference(
        lhs.cbegin(), lhs.cend(),
        rhs.cbegin(), rhs.cend(),
        std::inserter(result, result.end()));
    return result;
}

SystemSet operator+(const SystemSet& lhs, const SystemSet& rhs)
{
    SystemSet result;
    std::set_union(
        lhs.cbegin(), lhs.cend(),
        rhs.cbegin(), rhs.cend(),
        std::inserter(result, result.end()));
    return result;
}

} // namespace

struct PushSystemsSelectionModel::Private
{
    PushSystemsSelectionModel* const q;
    QnCloudSystemList systems;
    SystemSet selectedSystems;

    QHash<QString, int> systemRowHash;

    bool hasChanges = false;
    void setHasChanges(bool value);
};

void PushSystemsSelectionModel::Private::setHasChanges(bool value)
{
    if (hasChanges == value)
        return;

    hasChanges = value;
    emit q->hasChangesChanged();
}

//--------------------------------------------------------------------------------------------------

void PushSystemsSelectionModel::registerQmlType()
{
    qmlRegisterUncreatableType<PushSystemsSelectionModel>("Nx.Mobile", 1, 0,
        "PushSystemsSelectionModel", "Cannot create an instance of PushSystemsSelectionModel");
}

PushSystemsSelectionModel::PushSystemsSelectionModel(
    const QnCloudSystemList& systems,
    const QStringList& selectedSystems,
    QObject* parent)
    :
    base_type(parent),
    d(new Private{this})
{
    d->systems = sortedByName(systems);

    for (int row = 0; row != d->systems.size(); ++row)
        d->systemRowHash.insert(d->systems[row].cloudId, row);

    setSelectedSystems(selectedSystems);
}

PushSystemsSelectionModel::~PushSystemsSelectionModel()
{
}

QStringList PushSystemsSelectionModel::selectedSystems() const
{
    return QList(d->selectedSystems.cbegin(), d->selectedSystems.cend());
}

void PushSystemsSelectionModel::setSelectedSystems(const QStringList& value)
{
    const auto sortedValue = SystemSet(value.cbegin(), value.cend()) - allSystemsModeValue();
    if (d->selectedSystems == sortedValue)
        return;

    const auto removed = d->selectedSystems - sortedValue;
    const auto added = sortedValue - d->selectedSystems;
    d->selectedSystems = sortedValue;

    for (const auto& changedSystem: removed + added)
    {
        const auto it = d->systemRowHash.find(changedSystem);
        if (it == d->systemRowHash.end())
            return;

        const auto targetIndex = index(it.value());
        emit dataChanged(targetIndex, targetIndex, {Qt::CheckStateRole});
    }

    d->setHasChanges(false);
    emit selectedSystemsChanged();
}

bool PushSystemsSelectionModel::hasChanges() const
{
    return d->hasChanges;
}

void PushSystemsSelectionModel::resetChangesFlag()
{
    d->setHasChanges(false);
}

void PushSystemsSelectionModel::setCheckedState(int row, Qt::CheckState state)
{
    if (!qBetween(0, row, (int) d->systems.size()))
        return;

    const auto systemId = d->systems[row].cloudId;
    bool changed = false;
    if (state != Qt::Checked)
    {
        changed = d->selectedSystems.erase(systemId);
    }
    else if (!d->selectedSystems.contains(systemId))
    {
        changed = true;
        d->selectedSystems.insert(systemId);
    }

    if (!changed)
        return;

    d->setHasChanges(true);

    const auto targetIndex = index(row);
    emit dataChanged(targetIndex, targetIndex, {Qt::CheckStateRole});

    emit selectedSystemsChanged();
}

int PushSystemsSelectionModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : d->systems.size();
}

QVariant PushSystemsSelectionModel::data(const QModelIndex &index, int role) const
{
    const int row = index.row();
    if (!qBetween(0, row, (int) d->systems.size()) || index.parent().isValid())
        return QVariant();

    switch(role)
    {
        case SystemIdRole:
            return d->systems[row].cloudId;
        case Qt::DisplayRole:
            return d->systems[row].name;
        case Qt::CheckStateRole:
            return d->selectedSystems.contains(d->systems[row].cloudId)
                ? Qt::Checked
                : Qt::Unchecked;
        default:
            return QVariant();
    }
}

PushSystemsSelectionModel::RolesHash PushSystemsSelectionModel::roleNames() const
{
    static const nx::vms::client::mobile::details::PushSystemsSelectionModel::RolesHash kRolesHash = {
        {SystemIdRole, "systemId"},
        {Qt::DisplayRole, "systemName"},
        {Qt::CheckStateRole, "checkState"}};

    return kRolesHash;
}

} // namespace nx::vms::client::mobile::details
