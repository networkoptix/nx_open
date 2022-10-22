// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "custom_access_summary_model.h"

#include <algorithm>

#include <QtQml/QtQml>

namespace nx::vms::client::desktop {

using namespace nx::vms::api;

namespace {

struct PermissionFlagInfo
{
    std::variant<AccessRight, GlobalPermission> flag;
    QString text;
};

const std::vector<PermissionFlagInfo> permissionSummary()
{
    return {
        {AccessRight::view, CustomAccessSummaryModel::tr("View")},
        {AccessRight::viewArchive, CustomAccessSummaryModel::tr("View video archive")},
        {AccessRight::exportArchive, CustomAccessSummaryModel::tr("Export video archive")},
        {AccessRight::viewBookmarks, CustomAccessSummaryModel::tr("View bookmarks")},
        {AccessRight::manageBookmarks, CustomAccessSummaryModel::tr("Modify bookmarks")},
        {AccessRight::userInput, CustomAccessSummaryModel::tr("User input")},
        {AccessRight::edit, CustomAccessSummaryModel::tr("Edit camera settings")},
        {GlobalPermission::viewLogs, CustomAccessSummaryModel::tr("View event log")},
    };
};

} // namespace

CustomAccessSummaryModel::CustomAccessSummaryModel(QObject* parent):
    base_type(parent)
{
}

CustomAccessSummaryModel::~CustomAccessSummaryModel()
{
}

bool CustomAccessSummaryModel::testFlag(
    const std::variant<api::AccessRight, api::GlobalPermission>& flag) const
{
    if (const auto* accessRight = std::get_if<AccessRight>(&flag))
        return m_customRights.testFlag(*accessRight);

    if (const auto* globalPermission = std::get_if<GlobalPermission>(&flag))
        return m_ownGlobalPermissions.testFlag(*globalPermission);

    return false;
}

QVariant CustomAccessSummaryModel::data(const QModelIndex& index, int role) const
{
    const auto permissions = permissionSummary();

    if (index.row() < 0 || index.row() >= (int) permissions.size())
        return {};

    switch (role)
    {
        case Qt::DisplayRole:
            return permissions.at(index.row()).text;

        case Qt::CheckStateRole:
        {
            return testFlag(permissions.at(index.row()).flag)
                ? Qt::Checked
                : Qt::Unchecked;
        }
        default:
            return {};
    }
}

int CustomAccessSummaryModel::rowCount(const QModelIndex&) const
{
    return m_hasDisplayableItems ? permissionSummary().size() : 0;
}

QHash<int, QByteArray> CustomAccessSummaryModel::roleNames() const
{
    auto names = base_type::roleNames();

    names.insert(Qt::CheckStateRole, "checkState");

    return names;
}

void CustomAccessSummaryModel::setOwnGlobalPermissions(GlobalPermissions permissions)
{
    if (m_ownGlobalPermissions == permissions)
        return;

    m_ownGlobalPermissions = permissions;
    updateInfo();
}

void CustomAccessSummaryModel::setOwnSharedResources(
    const nx::core::access::ResourceAccessMap& resources)
{
    AccessRights customRights = {};

    for (const auto& accessRights: resources.values())
        customRights |= accessRights;

    if (customRights == m_customRights)
        return;

    m_customRights = customRights;
    updateInfo();
}

void CustomAccessSummaryModel::updateInfo()
{
    const auto permissions = permissionSummary();

    bool hasDisplayableItems = std::any_of(permissions.begin(), permissions.end(),
        [this](const auto& it)
        {
            return testFlag(it.flag);
        });

    if (hasDisplayableItems != m_hasDisplayableItems)
    {
        if (hasDisplayableItems)
        {
            beginInsertRows({}, 0, permissions.size() - 1);
            m_hasDisplayableItems = hasDisplayableItems;
            endInsertRows();
        }
        else
        {
            beginRemoveRows({}, 0, permissions.size() - 1);
            m_hasDisplayableItems = hasDisplayableItems;
            endRemoveRows();
        }
        return;
    }

    if (!m_hasDisplayableItems)
        return;

    emit dataChanged(
        this->index(0, 0),
        this->index(permissions.size() - 1, 0));
}

void CustomAccessSummaryModel::registerQmlTypes()
{
    qmlRegisterType<CustomAccessSummaryModel>(
        "nx.vms.client.desktop", 1, 0, "CustomAccessSummaryModel");
}

} // namespace nx::vms::client::desktop
