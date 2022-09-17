// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "deprecated_user_list_model.h"

#include <client/client_globals.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/user_resource.h>
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
#include <nx/vms/client/desktop/system_context_aware.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <ui/workbench/workbench_access_controller.h>

using namespace nx::vms::client::desktop;

class QnDeprecatedUserListModelPrivate:
    public QObject,
    public SystemContextAware
{
    Q_DECLARE_TR_FUNCTIONS(QnDeprecatedUserListModelPrivate)
    using base_type = QObject;

public:
    QnDeprecatedUserListModel* model;

    QnUserResourceList users;
    QSet<QnUserResourcePtr> checkedUsers;
    QHash<QnUserResourcePtr, bool> enableChangedUsers;
    QHash<QnUserResourcePtr, bool> digestChangedUsers;

    QnDeprecatedUserListModelPrivate(QnDeprecatedUserListModel* parent) :
        base_type(parent),
        SystemContextAware(parent->systemContext()),
        model(parent)
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

void QnDeprecatedUserListModelPrivate::at_resourcePool_resourceChanged(const QnResourcePtr& resource)
{
    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if (!user)
        return;
    handleUserChanged(user);
}

void QnDeprecatedUserListModelPrivate::handleUserChanged(const QnUserResourcePtr& user)
{
    int row = users.indexOf(user);
    if (row == -1)
        return;

    QModelIndex index = model->index(row);
    emit model->dataChanged(index, index.sibling(row, QnDeprecatedUserListModel::ColumnCount - 1));
}

QnUserResourcePtr QnDeprecatedUserListModelPrivate::user(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() >= users.size())
        return QnUserResourcePtr();

    return users[index.row()];
}

// TODO: #vkutin Move this function to more suitable place. Rewrite it if needed.
QString QnDeprecatedUserListModelPrivate::permissionsString(const QnUserResourcePtr& user) const
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

bool QnDeprecatedUserListModelPrivate::isUnique(const QnUserResourcePtr& user) const
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

Qt::CheckState QnDeprecatedUserListModelPrivate::checkState() const
{
    if (checkedUsers.isEmpty())
        return Qt::Unchecked;

    if (checkedUsers.size() == users.size())
        return Qt::Checked;

    return Qt::PartiallyChecked;
}

void QnDeprecatedUserListModelPrivate::setCheckState(
    Qt::CheckState state, const QnUserResourcePtr& user)
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

void QnDeprecatedUserListModelPrivate::resetUsers(const QnUserResourceList& value)
{
    model->beginResetModel();
    for (const auto& user: users)
        removeUserInternal(user);
    users = value;
    for (const auto& user: users)
        addUserInternal(user);
    model->endResetModel();
}

void QnDeprecatedUserListModelPrivate::addUser(const QnUserResourcePtr& user)
{
    if (users.contains(user))
        return;

    int row = users.size();
    model->beginInsertRows(QModelIndex(), row, row);
    users.append(user);
    model->endInsertRows();

    addUserInternal(user);
}

void QnDeprecatedUserListModelPrivate::removeUser(const QnUserResourcePtr& user)
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

void QnDeprecatedUserListModelPrivate::addUserInternal(const QnUserResourcePtr& user)
{
    connect(user.get(), &QnUserResource::nameChanged, this,
        &QnDeprecatedUserListModelPrivate::at_resourcePool_resourceChanged);
    connect(user.get(), &QnUserResource::fullNameChanged, this,
        &QnDeprecatedUserListModelPrivate::at_resourcePool_resourceChanged);
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

void QnDeprecatedUserListModelPrivate::removeUserInternal(const QnUserResourcePtr& user)
{
    disconnect(user.get(), nullptr, this, nullptr);
    checkedUsers.remove(user);
    enableChangedUsers.remove(user);
    digestChangedUsers.remove(user);
}

QnDeprecatedUserListModel::QnDeprecatedUserListModel(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new QnDeprecatedUserListModelPrivate(this))
{
}

QnDeprecatedUserListModel::~QnDeprecatedUserListModel()
{
}

int QnDeprecatedUserListModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
        return d->users.size();

    return 0;
}

int QnDeprecatedUserListModel::columnCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return ColumnCount;

    return 0;
}

QVariant QnDeprecatedUserListModel::data(const QModelIndex& index, int role) const
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
                case LoginColumn        : return user->getName();
                case FullNameColumn     : return user->fullName();
                case UserRoleColumn     : return userRolesManager()->userRoleName(user);
                default                 : break;

            } // switch (column)
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

                case UserRoleColumn:
                    return d->permissionsString(user);

                case EnabledColumn:
                    return user->isEnabled() ? tr("Enabled") : tr("Disabled");

                default:
                    return QString(); // not QVariant() because we want to hide a tooltip if shown

            } // switch (column)
            break;
        }

        case Qt::DecorationRole:
        {
            if (index.column() == UserTypeColumn)
            {
                switch (user->userType())
                {
                    case nx::vms::api::UserType::cloud:
                    {
                        return user->isEnabled()
                            ? qnSkin->icon("cloud/cloud_20.png")
                            : qnSkin->icon("cloud/cloud_20_disabled.png");
                    }
                    case nx::vms::api::UserType::ldap:
                        return qnSkin->icon("user_settings/user_type_ldap.png");
                    default:
                        break;
                }
            }

            break;
        }

        case Qt::ForegroundRole:
        {
            /* Always use default color for checkboxes. */
            if (index.column() == CheckBoxColumn)
                return QVariant();

            /* Highlight conflicting users. */
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

        // TODO: #vkutin Refactor this role.
        case Qn::DisabledRole:
        {
            if (index.column() == EnabledColumn)
                return !accessController()->hasPermissions(user, Qn::WriteAccessRightsPermission);
            break;
        }

        case Qt::CheckStateRole:
        {
            if (index.column() == CheckBoxColumn)
                return d->checkedUsers.contains(user) ? Qt::Checked : Qt::Unchecked;

            if (index.column() == EnabledColumn)
                return isUserEnabled(user) ? Qt::Checked : Qt::Unchecked;

            break;
        }

        default:
            break;

    } // switch (role)

    return QVariant();
}

QVariant QnDeprecatedUserListModel::headerData(
    int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Vertical)
        return QVariant();

    if (section >= ColumnCount)
        return QVariant();

    if (role != Qt::DisplayRole)
        return base_type::headerData(section, orientation, role);

    switch (section)
    {
        case LoginColumn        : return tr("Login");
        case FullNameColumn     : return tr("Name");
        case UserRoleColumn     : return tr("Role");

        case UserTypeColumn:
        case EnabledColumn:
        default:
            return QString();
    }
}

Qt::ItemFlags QnDeprecatedUserListModel::flags(const QModelIndex& index) const
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

Qt::CheckState QnDeprecatedUserListModel::checkState() const
{
    return d->checkState();
}

void QnDeprecatedUserListModel::setCheckState(Qt::CheckState state, const QnUserResourcePtr& user)
{
    if (state == Qt::PartiallyChecked)
        return;

    auto roles = QVector<int>() << Qt::CheckStateRole << Qt::BackgroundRole << Qt::ForegroundRole;

    d->setCheckState(state, user);
    if (!user)
    {
        emit dataChanged(index(0, CheckBoxColumn), index(d->users.size() - 1, ColumnCount - 1),
            roles);
    }
    else
    {
        auto row = d->users.indexOf(user);
        if (row >= 0)
            emit dataChanged(index(row, CheckBoxColumn), index(row, ColumnCount - 1), roles);
    }
}

bool QnDeprecatedUserListModel::isUserEnabled(const QnUserResourcePtr& user) const
{
    if (!d->enableChangedUsers.contains(user))
        return user->isEnabled();

    return d->enableChangedUsers[user];
}

void QnDeprecatedUserListModel::setUserEnabled(const QnUserResourcePtr& user, bool enabled)
{
    NX_ASSERT(user->resourcePool());
    if (!user->resourcePool())
        return;

    d->enableChangedUsers[user] = enabled;
    d->handleUserChanged(user);
}

bool QnDeprecatedUserListModel::isDigestEnabled(const QnUserResourcePtr& user) const
{
    return d->digestChangedUsers.contains(user)
        ? d->digestChangedUsers[user]
        : user->shouldDigestAuthBeUsed();
}

void QnDeprecatedUserListModel::setDigestEnabled(const QnUserResourcePtr& user, bool enabled)
{
    NX_ASSERT(!user->isCloud());
    d->digestChangedUsers[user] = enabled;
}

QnUserResourceList QnDeprecatedUserListModel::users() const
{
    return d->users;
}

void QnDeprecatedUserListModel::resetUsers(const QnUserResourceList& value)
{
    d->resetUsers(value);
}

void QnDeprecatedUserListModel::addUser(const QnUserResourcePtr& user)
{
    d->addUser(user);
}

void QnDeprecatedUserListModel::removeUser(const QnUserResourcePtr& user)
{
    d->removeUser(user);
}

bool QnDeprecatedUserListModel::isInteractiveColumn(int column)
{
    return column == CheckBoxColumn || column == EnabledColumn;
}

QnDeprecatedSortedUserListModel::QnDeprecatedSortedUserListModel(QObject* parent):
    base_type(parent)
{
}

void QnDeprecatedSortedUserListModel::setDigestFilter(std::optional<bool> value)
{
    m_digestFilter = value;
    invalidateFilter();
}

bool QnDeprecatedSortedUserListModel::lessThan(
    const QModelIndex& left, const QModelIndex& right) const
{
    QnUserResourcePtr leftUser = left.data(Qn::UserResourceRole).value<QnUserResourcePtr>();
    QnUserResourcePtr rightUser = right.data(Qn::UserResourceRole).value<QnUserResourcePtr>();

    NX_ASSERT(leftUser);
    NX_ASSERT(rightUser);

    if (!rightUser)
        return true;
    if (!leftUser)
        return false;

    switch (sortColumn())
    {
        case QnDeprecatedUserListModel::EnabledColumn:
        {
            bool leftEnabled = leftUser->isEnabled();
            bool rightEnabled = rightUser->isEnabled();
            if (leftEnabled != rightEnabled)
                return leftEnabled;

            break;
        }

        case QnDeprecatedUserListModel::UserTypeColumn:
        {
            const auto leftType = leftUser->userType();
            const auto rightType = rightUser->userType();
            if (leftType != rightType)
                return leftType < rightType;

            break;
        }

        case QnDeprecatedUserListModel::FullNameColumn:
        case QnDeprecatedUserListModel::UserRoleColumn:
        {
            QString leftText = left.data(Qt::DisplayRole).toString();
            QString rightText = right.data(Qt::DisplayRole).toString();

            if (leftText != rightText)
                return leftText < rightText;

            break;
        }

        default:
            break;
    }

    /* Otherwise sort by login (which is unique): */
    return nx::utils::naturalStringLess(leftUser->getName(), rightUser->getName());
}

bool QnDeprecatedSortedUserListModel::filterAcceptsRow(
    int sourceRow, const QModelIndex& sourceParent) const
{
    QModelIndex sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);
    const auto user = sourceIndex.data(Qn::UserResourceRole).value<QnUserResourcePtr>();

    return (!m_digestFilter || m_digestFilter == user->shouldDigestAuthBeUsed())
        && base_type::filterAcceptsRow(sourceRow, sourceParent);
}
