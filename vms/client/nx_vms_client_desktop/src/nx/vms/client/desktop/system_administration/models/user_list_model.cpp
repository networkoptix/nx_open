// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_list_model.h"

#include <client/client_globals.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/branding.h>
#include <nx/utils/qset.h>
#include <nx/utils/string.h>
#include <nx/vms/api/data/user_role_data.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <ui/workbench/workbench_access_controller.h>

namespace nx::vms::client::desktop {

namespace {

bool isCustomUser(const QnUserResourcePtr& user)
{
    if (!NX_ASSERT(user))
        return false;

    return user->getRawPermissions() != GlobalPermission::none
        || !user->systemContext()->accessRightsManager()->ownResourceAccessMap(
            user->getId()).isEmpty();
}

} // namespace

class UserListModel::Private:
    public QObject,
    public SystemContextAware
{
    Q_DECLARE_TR_FUNCTIONS(UserListModel)
    using base_type = QObject;

    UserListModel* const model;

public:
    QnUserResourceList users;
    QSet<QnUserResourcePtr> checkedUsers;
    QHash<QnUserResourcePtr, bool> enableChangedUsers;
    QHash<QnUserResourcePtr, bool> digestChangedUsers;

    Private(UserListModel* q):
        SystemContextAware(q->systemContext()),
        model(q)
    {
        connect(resourcePool(), &QnResourcePool::resourceAdded, this,
            [this](const QnResourcePtr& resource)
            {
                if (auto user = resource.dynamicCast<QnUserResource>())
                    addUser(user);
            });

        connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
            [this](const QnResourcePtr& resource)
            {
                if (auto user = resource.dynamicCast<QnUserResource>())
                    removeUser(user);
            });

        connect(userRolesManager(), &QnUserRolesManager::userRoleAddedOrUpdated, this,
            [this](const nx::vms::api::UserRoleData& userRole)
            {
                for (auto user: users)
                {
                    if (user->firstRoleId() != userRole.id)
                        continue;
                    handleUserChanged(user);
                }
            });

        connect(globalPermissionsManager(), &QnGlobalPermissionsManager::globalPermissionsChanged,
            this,
            [this](const QnResourceAccessSubject& subject, GlobalPermissions /*value*/)
            {
                if (subject.user())
                    handleUserChanged(subject.user());
            });
    }

    void at_resourcePool_resourceChanged(const QnResourcePtr& resource);

    void handleUserChanged(const QnUserResourcePtr& user);

    QnUserResourcePtr user(const QModelIndex& index) const;
    QString permissionsString(const QnUserResourcePtr& user) const;
    bool isUnique(const QnUserResourcePtr& user) const;

    Qt::CheckState checkState() const;
    void setCheckState(Qt::CheckState state, const QnUserResourcePtr& user = QnUserResourcePtr());

    void resetUsers(const QnUserResourceList& value);
    void addUser(const QnUserResourcePtr& user);
    void removeUser(const QnUserResourcePtr& user);

private:
    void addUserInternal(const QnUserResourcePtr& user);
    void removeUserInternal(const QnUserResourcePtr& user);
};

void UserListModel::Private::at_resourcePool_resourceChanged(const QnResourcePtr& resource)
{
    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if (!user)
        return;
    handleUserChanged(user);
}

void UserListModel::Private::handleUserChanged(const QnUserResourcePtr& user)
{
    int row = users.indexOf(user);
    if (row == -1)
        return;

    QModelIndex index = model->index(row);
    emit model->dataChanged(index, index.sibling(row, UserListModel::ColumnCount - 1));
}

QnUserResourcePtr UserListModel::Private::user(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() >= users.size())
        return QnUserResourcePtr();

    return users[index.row()];
}

// TODO: #vkutin Move this function to more suitable place. Rewrite it if needed.
QString UserListModel::Private::permissionsString(const QnUserResourcePtr& user) const
{
    QStringList permissionStrings;

    GlobalPermissions permissions = globalPermissionsManager()->globalPermissions(user);

    if (user->isOwner())
        return tr("Owner");

    if (permissions.testFlag(GlobalPermission::admin))
        return tr("Administrator");

    permissionStrings.append(tr("View live video"));

    if (permissions.testFlag(GlobalPermission::editCameras))
        permissionStrings.append(QnDeviceDependentStrings::getDefaultNameFromSet(
            resourcePool(),
            tr("Adjust device settings"),
            tr("Adjust camera settings")
        ));

    if (permissions.testFlag(GlobalPermission::userInput))
        permissionStrings.append(tr("Use PTZ controls"));

    if (permissions.testFlag(GlobalPermission::viewArchive))
        permissionStrings.append(tr("View video archives"));

    if (permissions.testFlag(GlobalPermission::exportArchive))
        permissionStrings.append(tr("Export video"));

    if (permissions.testFlag(GlobalPermission::controlVideowall))
        permissionStrings.append(tr("Control Video Walls"));

    return permissionStrings.join(lit(", "));
}

bool UserListModel::Private::isUnique(const QnUserResourcePtr& user) const
{
    QString userName = user->getName();
    for (const QnUserResourcePtr& other : users)
    {
        if (other == user)
            continue;

        if (other->getName().compare(userName, Qt::CaseInsensitive) == 0)
            return false;
    }
    return true;
}

Qt::CheckState UserListModel::Private::checkState() const
{
    if (checkedUsers.isEmpty())
        return Qt::Unchecked;

    if (checkedUsers.size() == users.size())
        return Qt::Checked;

    return Qt::PartiallyChecked;
}

void UserListModel::Private::setCheckState(Qt::CheckState state, const QnUserResourcePtr& user)
{
    if (!user)
    {
        if (state == Qt::Checked)
            checkedUsers = nx::utils::toQSet(users);
        else if (state == Qt::Unchecked)
            checkedUsers.clear();
    }
    else
    {
        if (state == Qt::Checked)
            checkedUsers.insert(user);
        else if (state == Qt::Unchecked)
            checkedUsers.remove(user);
    }
}

void UserListModel::Private::resetUsers(const QnUserResourceList& value)
{
    model->beginResetModel();
    for (const auto& user: users)
        removeUserInternal(user);
    users = value;
    for (const auto& user: users)
        addUserInternal(user);
    model->endResetModel();
}

void UserListModel::Private::addUser(const QnUserResourcePtr& user)
{
    if (users.contains(user))
        return;

    int row = users.size();
    model->beginInsertRows(QModelIndex(), row, row);
    users.append(user);
    model->endInsertRows();

    addUserInternal(user);
}

void UserListModel::Private::removeUser(const QnUserResourcePtr& user)
{
    int row = users.indexOf(user);
    if (row >= 0)
    {
        model->beginRemoveRows(QModelIndex(), row, row);
        users.removeAt(row);
        model->endRemoveRows();
    }

    removeUserInternal(user);
}

void UserListModel::Private::addUserInternal(const QnUserResourcePtr& user)
{
    connect(user.get(), &QnUserResource::nameChanged, this,
        &UserListModel::Private::at_resourcePool_resourceChanged);
    connect(user.get(), &QnUserResource::fullNameChanged, this,
        &UserListModel::Private::at_resourcePool_resourceChanged);
    connect(user.get(), &QnUserResource::enabledChanged, this,
        [this](const QnUserResourcePtr &user)
        {
            enableChangedUsers.remove(user);
            handleUserChanged(user);
        });
    connect(user.get(), &QnUserResource::digestChanged,
        this,
        [this](const QnUserResourcePtr& user)
        {
            digestChangedUsers.remove(user);
            handleUserChanged(user);
        });
}

void UserListModel::Private::removeUserInternal(const QnUserResourcePtr& user)
{
    disconnect(user.get(), nullptr, this, nullptr);
    checkedUsers.remove(user);
    enableChangedUsers.remove(user);
    digestChangedUsers.remove(user);
}

UserListModel::UserListModel(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent, QnWorkbenchContextAware::InitializationMode::lazy),
    d(new UserListModel::Private(this))
{
    const auto users = resourcePool()->getResources<QnUserResource>();
    for (const auto& user: users)
        d->addUser(user);
}

UserListModel::~UserListModel()
{
    // Required here for forward-declared scoped pointer destruction.
}

int UserListModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
        return d->users.size();

    return 0;
}

int UserListModel::columnCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return ColumnCount;

    return 0;
}

QVariant UserListModel::data(const QModelIndex& index, int role) const
{
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    QnUserResourcePtr user = d->users[index.row()];

    switch (role)
    {
        case Qt::DisplayRole:
        {
            switch (index.column())
            {
                case LoginColumn:
                    return user->getName();
                case FullNameColumn:
                    return user->fullName();
                case EmailColumn:
                    return user->getEmail();
                case UserGroupsColumn:
                    return userRolesManager()->userRoleName(user);
                default:
                    break;
            }
            break;
        }

        case Qt::ToolTipRole:
        {
            switch (index.column())
            {
                case UserTypeColumn:
                {
                    switch (user->userType())
                    {
                        case nx::vms::api::UserType::local:
                            return tr("Local user");
                        case nx::vms::api::UserType::cloud:
                            return tr("%1 user", "%1 is the short cloud name (like Cloud)")
                                .arg(nx::branding::shortCloudName());
                        case nx::vms::api::UserType::ldap:
                            return tr("LDAP user");
                        default:
                            break;
                    }

                    break;
                }

                case LoginColumn:
                    return user->getName();

                case FullNameColumn:
                    return user->fullName();

                case EmailColumn:
                    return user->getEmail();

                case UserGroupsColumn:
                    return d->permissionsString(user);

                default:
                    return QString(); // not QVariant() because we want to hide a tooltip if shown.

            } // switch (column)
            break;
        }

        case Qn::DecorationPathRole:
        {
            switch (index.column())
            {
                case UserTypeColumn:
                {
                    switch (user->userType())
                    {
                        case nx::vms::api::UserType::cloud:
                            return user->isEnabled()
                                ? QString("cloud/cloud_20.png")
                                : QString("cloud/cloud_20_disabled.png");

                        case nx::vms::api::UserType::ldap:
                            return QString("user_settings/user_type_ldap.png");

                        default:
                            break;
                    }

                    break;
                }

                case IsCustomColumn:
                {
                    if (isCustomUser(user))
                        return QString("text_buttons/ok.svg");

                    break;
                }

                default:
                    break;
            }

            break;
        }

        case Qt::DecorationRole:
        {
            const auto path = data(index, Qn::DecorationPathRole).toString();
            return path.isEmpty() ? QVariant() : QVariant::fromValue(qnSkin->icon(path));
        }

        case Qt::ForegroundRole:
        {
            // Always use default color for checkboxes.
            if (index.column() == CheckBoxColumn)
                return QVariant();

            // Highlight conflicting users.
            if (user->isLdap() && !d->isUnique(user))
                return colorTheme()->color("red_l2");

            break;
        }

        case Qn::UserResourceRole:
            return QVariant::fromValue(user);

        case Qt::TextAlignmentRole:
        {
            if (index.column() == UserTypeColumn)
                return Qt::AlignCenter;
            break;
        }

        case Qn::DisabledRole:
            return index.column() != CheckBoxColumn && !isUserEnabled(user);

        case Qt::CheckStateRole:
        {
            if (index.column() == CheckBoxColumn)
                return d->checkedUsers.contains(user) ? Qt::Checked : Qt::Unchecked;
            break;
        }

        default:
            break;
    }

    return QVariant();
}

QVariant UserListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Vertical)
        return QVariant();

    if (section >= ColumnCount)
        return QVariant();

    if (role != Qt::DisplayRole)
        return base_type::headerData(section, orientation, role);

    switch (section)
    {
        case LoginColumn:
            return tr("Login");
        case FullNameColumn:
            return tr("Name");
        case EmailColumn:
            return tr("Email");
        case UserGroupsColumn:
            return tr("Groups");
        case IsCustomColumn:
            return tr("Custom");
        default:
            return QString();
    }
}

Qt::ItemFlags UserListModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags flags = Qt::NoItemFlags;

    QnUserResourcePtr user = d->user(index);
    if (!user)
        return flags;

    flags |= Qt::ItemIsSelectable | Qt::ItemIsEnabled;

    if (index.column() == CheckBoxColumn)
        flags |= Qt::ItemIsUserCheckable;

    return flags;
}

QHash<int, QByteArray> UserListModel::roleNames() const
{
    auto roleNames = base_type::roleNames();
    roleNames[Qt::CheckStateRole] = "checkState";
    roleNames[Qn::DecorationPathRole] = "decorationPath";
    return roleNames;
}

Qt::CheckState UserListModel::checkState() const
{
    return d->checkState();
}

void UserListModel::setCheckState(Qt::CheckState state, const QnUserResourcePtr& user)
{
    if (state == Qt::PartiallyChecked)
        return;

    auto roles = QVector<int>() << Qt::CheckStateRole << Qt::BackgroundRole << Qt::ForegroundRole;

    d->setCheckState(state, user);
    if (!user)
    {
        emit dataChanged(index(0, CheckBoxColumn), index(d->users.size() - 1, ColumnCount - 1), roles);
    }
    else
    {
        auto row = d->users.indexOf(user);
        if (row >= 0)
            emit dataChanged(index(row, CheckBoxColumn), index(row, ColumnCount - 1), roles);
    }
}

bool UserListModel::isUserEnabled(const QnUserResourcePtr& user) const
{
    if (!d->enableChangedUsers.contains(user))
        return user->isEnabled();

    return d->enableChangedUsers[user];
}

void UserListModel::setUserEnabled(const QnUserResourcePtr& user, bool enabled)
{
    NX_ASSERT(user->resourcePool());
    if (!user->resourcePool())
        return;

    d->enableChangedUsers[user] = enabled;
    d->handleUserChanged(user);
}

bool UserListModel::isDigestEnabled(const QnUserResourcePtr& user) const
{
    return d->digestChangedUsers.contains(user)
        ? d->digestChangedUsers[user]
        : user->shouldDigestAuthBeUsed();
}

void UserListModel::setDigestEnabled(const QnUserResourcePtr& user, bool enabled)
{
    NX_ASSERT(!user->isCloud());
    d->digestChangedUsers[user] = enabled;
}

QnUserResourceList UserListModel::users() const
{
    return d->users;
}

void UserListModel::resetUsers(const QnUserResourceList& value)
{
    d->resetUsers(value);
}

void UserListModel::addUser(const QnUserResourcePtr& user)
{
    d->addUser(user);
}

void UserListModel::removeUser(const QnUserResourcePtr& user)
{
    d->removeUser(user);
}

bool UserListModel::isInteractiveColumn(int column)
{
    return column == CheckBoxColumn;
}

SortedUserListModel::SortedUserListModel(QObject *parent) : base_type(parent)
{
}

void SortedUserListModel::setDigestFilter(std::optional<bool> value)
{
    m_digestFilter = value;
    invalidateFilter();
}

bool SortedUserListModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const auto leftUser = left.data(Qn::UserResourceRole).value<QnUserResourcePtr>();
    const auto rightUser = right.data(Qn::UserResourceRole).value<QnUserResourcePtr>();

    if (!NX_ASSERT(rightUser))
        return true;

    if (!NX_ASSERT(leftUser))
        return false;

    switch (sortColumn())
    {
        case UserListModel::UserTypeColumn:
        {
            const auto leftType = leftUser->userType();
            const auto rightType = rightUser->userType();
            if (leftType != rightType)
                return leftType < rightType;

            break;
        }

        case UserListModel::FullNameColumn:
        case UserListModel::EmailColumn:
        case UserListModel::UserGroupsColumn:
        {
            const QString leftText = left.data(Qt::DisplayRole).toString();
            const QString rightText = right.data(Qt::DisplayRole).toString();

            if (leftText != rightText)
                return leftText < rightText;

            break;
        }

        case UserListModel::IsCustomColumn:
        {
            const bool leftCustom = isCustomUser(leftUser);
            const bool rightCustom = isCustomUser(rightUser);
            if (leftCustom != rightCustom)
                return leftCustom;

            break;
        }

        default:
            break;
    }

    // Otherwise sort by login (which is unique):
    return nx::utils::naturalStringLess(leftUser->getName(), rightUser->getName());
}

bool SortedUserListModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    QModelIndex sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);
    const auto user = sourceIndex.data(Qn::UserResourceRole).value<QnUserResourcePtr>();

    return (!m_digestFilter || m_digestFilter == user->shouldDigestAuthBeUsed())
        && base_type::filterAcceptsRow(sourceRow, sourceParent);
}

} // namespace nx::vms::client::desktop
