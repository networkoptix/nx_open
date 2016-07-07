#include "user_list_model.h"

#include <core/resource/user_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_access_manager.h>

#include <ui/style/skin.h>
#include <ui/style/globals.h>
#include <ui/workbench/workbench_access_controller.h>
#include <nx/utils/string.h>

class QnUserListModelPrivate : public Connective<QObject>
{
    Q_DECLARE_TR_FUNCTIONS(QnUserListModelPrivate);

    typedef Connective<QObject> base_type;

public:
    QnUserListModel* model;

    QnUserResourceList userList;
    QSet<QnUserResourcePtr> checkedUsers;

    QnUserListModelPrivate(QnUserListModel* parent) :
        base_type(parent),
        model(parent)
    {
        userList = qnResPool->getResources<QnUserResource>();

        connect(qnResPool, &QnResourcePool::resourceAdded,   this, &QnUserListModelPrivate::at_resourcePool_resourceAdded);
        connect(qnResPool, &QnResourcePool::resourceRemoved, this, &QnUserListModelPrivate::at_resourcePool_resourceRemoved);
        connect(qnResPool, &QnResourcePool::resourceChanged, this, &QnUserListModelPrivate::at_resourcePool_resourceChanged);

        connect(qnResourceAccessManager, &QnResourceAccessManager::userGroupAddedOrUpdated, this,
            [this](const ec2::ApiUserGroupData& group)
            {
                auto isUserAffected = [&group](const QnUserResourcePtr& user)
                {
                    return user->userGroup() == group.id;
                };

                auto begin = std::find_if(userList.begin(), userList.end(), isUserAffected);
                if (begin == userList.end())
                    return;

                auto end = std::find_if(userList.rbegin(), QnUserResourceList::reverse_iterator(begin), isUserAffected).base();

                QModelIndex first = model->index(begin - userList.begin(), QnUserListModel::UserRoleColumn);
                QModelIndex last = first.sibling(first.row() + (end - begin) - 1, QnUserListModel::UserRoleColumn);

                emit model->dataChanged(first, last, { Qt::DisplayRole });
        });

        for (const QnUserResourcePtr& user: userList)
            at_resourcePool_resourceAdded(user);
    }

    void at_resourcePool_resourceAdded(const QnResourcePtr& resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr& resource);
    void at_resourcePool_resourceChanged(const QnResourcePtr& resource);

    int userIndex(const QnUuid& id) const;
    QnUserResourcePtr user(const QModelIndex& index) const;
    QString permissionsString(const QnUserResourcePtr& user) const;
    bool isUnique(const QnUserResourcePtr& user) const;

    Qt::CheckState checkState() const;
    void setCheckState(Qt::CheckState state, const QnUserResourcePtr& user = QnUserResourcePtr());
};


void QnUserListModelPrivate::at_resourcePool_resourceAdded(const QnResourcePtr& resource)
{
    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if (!user)
        return;

    connect(user, &QnUserResource::nameChanged,        this, &QnUserListModelPrivate::at_resourcePool_resourceChanged);
    connect(user, &QnUserResource::fullNameChanged,    this, &QnUserListModelPrivate::at_resourcePool_resourceChanged);
    connect(user, &QnUserResource::permissionsChanged, this, &QnUserListModelPrivate::at_resourcePool_resourceChanged);
    connect(user, &QnUserResource::enabledChanged,     this, &QnUserListModelPrivate::at_resourcePool_resourceChanged);

    if (userIndex(user->getId()) != -1)
        return;

    int row = userList.size();
    model->beginInsertRows(QModelIndex(), row, row);
    userList.append(user);
    model->endInsertRows();
}

void QnUserListModelPrivate::at_resourcePool_resourceRemoved(const QnResourcePtr& resource)
{
    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if (!user)
        return;

    disconnect(user, nullptr, this, nullptr);

    int row = userIndex(user->getId());
    if (row == -1)
        return;

    model->beginRemoveRows(QModelIndex(), row, row);
    userList.removeAt(row);
    checkedUsers.remove(user);
    model->endRemoveRows();
}

void QnUserListModelPrivate::at_resourcePool_resourceChanged(const QnResourcePtr& resource)
{
    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if (!user)
        return;

    int row = userIndex(user->getId());
    if (row == -1)
        return;

    QModelIndex index = model->index(row);
    emit model->dataChanged(index, index.sibling(row, QnUserListModel::ColumnCount - 1));
}

int QnUserListModelPrivate::userIndex(const QnUuid& id) const
{
    auto it = std::find_if(userList.begin(), userList.end(), [&id](const QnUserResourcePtr& user)
    {
        return user->getId() == id;
    });

    if (it == userList.end())
        return -1;

    return std::distance(userList.begin(), it);
}

QnUserResourcePtr QnUserListModelPrivate::user(const QModelIndex& index) const
{
    if (index.row() >= userList.size())
        return QnUserResourcePtr();

    return userList[index.row()];
}

//TODO: #vkutin #common Move this function to more suitable place. Rewrite it if needed.
QString QnUserListModelPrivate::permissionsString(const QnUserResourcePtr& user) const
{
    QStringList permissionStrings;

    Qn::GlobalPermissions permissions = qnResourceAccessManager->globalPermissions(user);

    if (user->isOwner())
        return tr("Owner");

    if (permissions.testFlag(Qn::GlobalAdminPermission))
        return tr("Administrator");

    permissionStrings.append(tr("View live video"));

    if (permissions.testFlag(Qn::GlobalEditCamerasPermission))
        permissionStrings.append(QnDeviceDependentStrings::getDefaultNameFromSet(
            tr("Adjust device settings"),
            tr("Adjust camera settings")
        ));

    if (permissions.testFlag(Qn::GlobalUserInputPermission))
        permissionStrings.append(tr("Use PTZ controls"));

    if (permissions.testFlag(Qn::GlobalViewArchivePermission))
        permissionStrings.append(tr("View video archives"));

    if (permissions.testFlag(Qn::GlobalExportPermission))
        permissionStrings.append(tr("Export video"));

    if (permissions.testFlag(Qn::GlobalControlVideoWallPermission))
        permissionStrings.append(tr("Control Video Walls"));

    return permissionStrings.join(lit(", "));
}

bool QnUserListModelPrivate::isUnique(const QnUserResourcePtr& user) const
{
    QString userName = user->getName();
    for (const QnUserResourcePtr &user1 : userList)
    {
        if (user1 == user)
            continue;

        if (user1->getName().compare(userName, Qt::CaseInsensitive) == 0)
            return false;
    }
    return true;
}

Qt::CheckState QnUserListModelPrivate::checkState() const
{
    if (checkedUsers.isEmpty())
        return Qt::Unchecked;

    if (checkedUsers.size() == userList.size())
        return Qt::Checked;

    return Qt::PartiallyChecked;
}

void QnUserListModelPrivate::setCheckState(Qt::CheckState state, const QnUserResourcePtr& user)
{
    if (!user)
    {
        if (state == Qt::Checked)
            checkedUsers = userList.toSet();
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

QnUserListModel::QnUserListModel(QObject* parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new QnUserListModelPrivate(this))
{
}

QnUserListModel::~QnUserListModel()
{
}

int QnUserListModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
        return d->userList.size();

    return 0;
}

int QnUserListModel::columnCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return ColumnCount;

    return 0;
}

QVariant QnUserListModel::data(const QModelIndex& index, int role) const
{
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    QnUserResourcePtr user = d->userList[index.row()];

    switch (role)
    {
        case Qt::DisplayRole:
        {
            switch (index.column())
            {
                case LoginColumn        : return user->getName();
                case FullNameColumn     : return user->fullName();
                case UserRoleColumn     : return accessController()->userRoleName(user);
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
                        case QnUserType::Local  : return tr("Local user");
                        case QnUserType::Cloud  : return tr("Cloud user");
                        case QnUserType::Ldap   : return tr("LDAP user");
                        default                 : break;
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
                    return QString(); // not QVariant() because we want to hide a tooltip if one is shown

            } // switch (column)
            break;
        }

        case Qt::DecorationRole:
        {
            if (index.column() == UserTypeColumn)
            {
                switch (user->userType())
                {
                    case QnUserType::Cloud  : return qnSkin->icon("misc/user_type_cloud.png");
                    case QnUserType::Ldap   : return qnSkin->icon("misc/user_type_ldap.png");
                    default                 : break;
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
                return qnGlobals->errorTextColor();

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

        //TODO: #vkutin #common Refactor this role
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
                return user->isEnabled() ? Qt::Checked : Qt::Unchecked;

            break;
        }

        default:
            break;

    } // switch (role)

    return QVariant();
}

QVariant QnUserListModel::headerData(int section, Qt::Orientation orientation, int role) const
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

Qt::ItemFlags QnUserListModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags flags = Qt::NoItemFlags;

    QnUserResourcePtr user = d->user(index);
    if (!user)
        return flags;

    flags |= Qt::ItemIsSelectable;

    if (isInteractiveColumn(index.column()) || user->isEnabled())
        flags |= Qt::ItemIsEnabled;

    if (index.column() == CheckBoxColumn)
        flags |= Qt::ItemIsUserCheckable;

    return flags;
}

Qt::CheckState QnUserListModel::checkState() const
{
    return d->checkState();
}

void QnUserListModel::setCheckState(Qt::CheckState state, const QnUserResourcePtr& user)
{
    if (state == Qt::PartiallyChecked)
        return;

    auto roles = QVector<int>() << Qt::CheckStateRole << Qt::BackgroundRole << Qt::ForegroundRole;

    d->setCheckState(state, user);
    if (!user)
    {
        emit dataChanged(index(0, CheckBoxColumn), index(d->userList.size() - 1, ColumnCount - 1), roles);
    }
    else
    {
        auto row = d->userIndex(user->getId());
        if (row >= 0)
            emit dataChanged(index(row, CheckBoxColumn), index(row, ColumnCount - 1), roles);
    }
}

bool QnUserListModel::isInteractiveColumn(int column)
{
    return column == CheckBoxColumn || column == EnabledColumn;
}

QnSortedUserListModel::QnSortedUserListModel(QObject *parent) : base_type(parent)
{
}

bool QnSortedUserListModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
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
        case QnUserListModel::EnabledColumn:
        {
            bool leftEnabled = leftUser->isEnabled();
            bool rightEnabled = rightUser->isEnabled();
            if (leftEnabled != rightEnabled)
                return leftEnabled;

            break;
        }

        case QnUserListModel::UserTypeColumn:
        {
            QnUserType leftType = leftUser->userType();
            QnUserType rightType = rightUser->userType();
            if (leftType != rightType)
                return leftType < rightType;

            break;
        }

        case QnUserListModel::FullNameColumn:
        case QnUserListModel::UserRoleColumn:
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
