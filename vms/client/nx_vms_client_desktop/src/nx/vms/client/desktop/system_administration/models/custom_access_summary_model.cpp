// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "custom_access_summary_model.h"

#include <algorithm>
#include <variant>

#include <QtQml/QtQml>

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

using namespace nx::vms::api;

struct CustomAccessSummaryModel::Private
{
    struct PermissionFlagInfo
    {
        std::variant<AccessRight, GlobalPermission> flag;
        QString text;
    };

    CustomAccessSummaryModel* const model;
    AccessRights customRights;
    GlobalPermissions ownGlobalPermissions;
    bool hasDisplayableItems = false;
    const std::vector<PermissionFlagInfo> permissions;

    bool testFlag(const std::variant<AccessRight, GlobalPermission>& flag) const;
    void updateInfo();
};

CustomAccessSummaryModel::CustomAccessSummaryModel(QObject* parent):
    base_type(parent),
    d(new Private{
        .model = this,
        .permissions = {
            {AccessRight::view, tr("View")},
            {AccessRight::viewArchive, tr("View video archive")},
            {AccessRight::exportArchive, tr("Export video archive")},
            {AccessRight::viewBookmarks, tr("View bookmarks")},
            {AccessRight::manageBookmarks, tr("Modify bookmarks")},
            {AccessRight::userInput, tr("User input")},
            {AccessRight::edit, tr("Edit camera settings")},
            {GlobalPermission::viewLogs, tr("View event log")}, //< Another enum is OK here.
        }})
{
}

CustomAccessSummaryModel::~CustomAccessSummaryModel()
{
}

bool CustomAccessSummaryModel::Private::testFlag(
    const std::variant<AccessRight, GlobalPermission>& flag) const
{
    if (const auto* accessRight = std::get_if<AccessRight>(&flag))
        return customRights.testFlag(*accessRight);

    if (const auto* globalPermission = std::get_if<GlobalPermission>(&flag))
        return ownGlobalPermissions.testFlag(*globalPermission);

    NX_ASSERT(false, "Unmatched flag type");

    return false;
}

QVariant CustomAccessSummaryModel::data(const QModelIndex& index, int role) const
{
    if (index.row() < 0 || index.row() >= (int) d->permissions.size())
        return {};

    switch (role)
    {
        case Qt::DisplayRole:
            return d->permissions.at(index.row()).text;

        case Qt::CheckStateRole:
        {
            return d->testFlag(d->permissions.at(index.row()).flag)
                ? Qt::Checked
                : Qt::Unchecked;
        }
        default:
            return {};
    }
}

int CustomAccessSummaryModel::rowCount(const QModelIndex&) const
{
    return d->hasDisplayableItems ? d->permissions.size() : 0;
}

QHash<int, QByteArray> CustomAccessSummaryModel::roleNames() const
{
    auto names = base_type::roleNames();

    names.insert(Qt::CheckStateRole, "checkState");

    return names;
}

void CustomAccessSummaryModel::setOwnGlobalPermissions(GlobalPermissions permissions)
{
    if (d->ownGlobalPermissions == permissions)
        return;

    d->ownGlobalPermissions = permissions;
    d->updateInfo();
}

void CustomAccessSummaryModel::setOwnSharedResources(
    const nx::core::access::ResourceAccessMap& resources)
{
    AccessRights customRights = {};

    for (const auto& accessRights: resources.values())
        customRights |= accessRights;

    if (customRights == d->customRights)
        return;

    d->customRights = customRights;
    d->updateInfo();
}

void CustomAccessSummaryModel::Private::updateInfo()
{
    const bool hasEnabledPermissions = std::any_of(permissions.begin(), permissions.end(),
        [this](const auto& it)
        {
            return testFlag(it.flag);
        });

    if (hasEnabledPermissions != hasDisplayableItems)
    {
        if (hasDisplayableItems)
        {
            model->beginInsertRows({}, 0, permissions.size() - 1);
            hasDisplayableItems = hasEnabledPermissions;
            model->endInsertRows();
        }
        else
        {
            model->beginRemoveRows({}, 0, permissions.size() - 1);
            hasDisplayableItems = hasEnabledPermissions;
            model->endRemoveRows();
        }
        return;
    }

    if (!hasDisplayableItems)
        return;

    emit model->dataChanged(
        model->index(0, 0),
        model->index(permissions.size() - 1, 0));
}

void CustomAccessSummaryModel::registerQmlTypes()
{
    qmlRegisterType<CustomAccessSummaryModel>(
        "nx.vms.client.desktop", 1, 0, "CustomAccessSummaryModel");
}

} // namespace nx::vms::client::desktop
