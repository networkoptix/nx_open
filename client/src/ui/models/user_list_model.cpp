#include "user_list_model.h"

#include <core/resource/user_resource.h>
#include <core/resource/resource_name.h>
#include <core/resource_management/resource_pool.h>

#include <common/user_permissions.h>
#include <ui/style/skin.h>
#include <ui/style/globals.h>
#include <ui/workbench/workbench_access_controller.h>
#include <utils/common/string.h>

class QnUserListModelPrivate : public Connective<QObject> {
    Q_DECLARE_TR_FUNCTIONS(QnUserListModelPrivate);

    typedef Connective<QObject> base_type;
public:
    QnUserListModel *model;

    QnUserResourceList userList;
    QSet<QnUserResourcePtr> checkedUsers;
    QnUserManagementColors colors;

    QnUserListModelPrivate(QnUserListModel *parent)
        : base_type(parent)
        , model(parent)
    {
        userList = qnResPool->getResources<QnUserResource>();

        connect(qnResPool, &QnResourcePool::resourceAdded, this, &QnUserListModelPrivate::at_resourcePool_resourceAdded);
        connect(qnResPool, &QnResourcePool::resourceRemoved, this, &QnUserListModelPrivate::at_resourcePool_resourceRemoved);
        connect(qnResPool, &QnResourcePool::resourceChanged, this, &QnUserListModelPrivate::at_resourcePool_resourceChanged);
    }

    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);
    void at_resourcePool_resourceChanged(const QnResourcePtr &resource);

    int userIndex(const QnUuid &id) const;
    QnUserResourcePtr user(const QModelIndex &index) const;
    QString permissionsString(const QnUserResourcePtr &user) const;
    bool isUnique(const QnUserResourcePtr &user) const;

    Qt::CheckState checkState() const;
    void setCheckState(Qt::CheckState state, const QnUserResourcePtr &user = QnUserResourcePtr());
};


void QnUserListModelPrivate::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if (!user)
        return;

    if (userIndex(user->getId()) != -1)
        return;

    connect(user,   &QnUserResource::nameChanged,           this,   &QnUserListModelPrivate::at_resourcePool_resourceChanged);
    connect(user,   &QnUserResource::permissionsChanged,    this,   &QnUserListModelPrivate::at_resourcePool_resourceChanged);
    connect(user,   &QnUserResource::enabledChanged,        this,   &QnUserListModelPrivate::at_resourcePool_resourceChanged);
    connect(user,   &QnUserResource::ldapChanged,           this,   &QnUserListModelPrivate::at_resourcePool_resourceChanged);

    int row = userList.size();
    model->beginInsertRows(QModelIndex(), row, row);
    userList.append(user);
    model->endInsertRows();
}

void QnUserListModelPrivate::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if (!user)
        return;

    int row = userIndex(user->getId());
    if (row == -1)
        return;

    disconnect(user, nullptr, this, nullptr);

    model->beginRemoveRows(QModelIndex(), row, row);
    userList.removeAt(row);
    checkedUsers.remove(user);
    model->endRemoveRows();
}

void QnUserListModelPrivate::at_resourcePool_resourceChanged(const QnResourcePtr &resource) {
    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if (!user)
        return;

    int row = userIndex(user->getId());
    if (row == -1)
        return;

    QModelIndex index = model->index(row);
    emit model->dataChanged(index, index.sibling(row, QnUserListModel::ColumnCount - 1));
}

int QnUserListModelPrivate::userIndex(const QnUuid &id) const {
    auto it = std::find_if(userList.begin(), userList.end(), [&id](const QnUserResourcePtr &user) {
        return user->getId() == id;
    });

    if (it == userList.end())
        return -1;

    return std::distance(userList.begin(), it);
}

QnUserResourcePtr QnUserListModelPrivate::user(const QModelIndex &index) const {
    if (index.row() >= userList.size())
        return QnUserResourcePtr();

    return userList[index.row()];
}

QString QnUserListModelPrivate::permissionsString(const QnUserResourcePtr &user) const {
    QStringList permissionStrings;

    qint64 permissions = model->accessController()->globalPermissions(user);

    if (permissions & Qn::GlobalEditProtectedUserPermission)
        permissionStrings.append(tr("Owner"));

    if (permissions & (Qn::GlobalProtectedPermission | Qn::GlobalEditProtectedUserPermission))
        permissionStrings.append(tr("Administrator"));

    if ((permissions & Qn::GlobalViewLivePermission) && permissionStrings.isEmpty())
        permissionStrings.append(tr("View live video"));
    if (permissions & Qn::GlobalEditCamerasPermission)
        permissionStrings.append(tr("Adjust %1 settings").arg(getDefaultDeviceNameLower()));
    if (permissions & Qn::GlobalPtzControlPermission)
        permissionStrings.append(tr("Use PTZ controls"));
    if (permissions & Qn::GlobalViewArchivePermission)
        permissionStrings.append(tr("View video archives"));
    if (permissions & Qn::GlobalExportPermission)
        permissionStrings.append(tr("Export video"));
    if (permissions & Qn::GlobalEditVideoWallPermission)
        permissionStrings.append(tr("Edit Video Walls"));

    return permissionStrings.join(lit(", "));
}

bool QnUserListModelPrivate::isUnique(const QnUserResourcePtr &user) const {
    QString userName = user->getName();
    for (const QnUserResourcePtr &user1 : userList) {
        if (user1 == user)
            continue;

        if (user1->getName().compare(userName, Qt::CaseInsensitive) == 0)
            return false;
    }
    return true;
}

Qt::CheckState QnUserListModelPrivate::checkState() const {
    if (checkedUsers.isEmpty())
        return Qt::Unchecked;

    if (checkedUsers.size() == userList.size())
        return Qt::Checked;
    
    return Qt::PartiallyChecked;
}

void QnUserListModelPrivate::setCheckState(Qt::CheckState state, const QnUserResourcePtr &user) {
    if (!user) {
        if (state == Qt::Checked)
            checkedUsers = userList.toSet();
        else if (state == Qt::Unchecked)
            checkedUsers.clear();
    } else {
        if (state == Qt::Checked)
            checkedUsers.insert(user);
        else if (state == Qt::Unchecked)
            checkedUsers.remove(user);
    }
}

QnUserListModel::QnUserListModel(QObject *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
    , d(new QnUserListModelPrivate(this))
{
}

QnUserListModel::~QnUserListModel() {}

int QnUserListModel::rowCount(const QModelIndex &parent) const {
    if (!parent.isValid())
        return d->userList.size();
    return 0;
}

int QnUserListModel::columnCount(const QModelIndex &parent) const {
    if (!parent.isValid())
        return ColumnCount;
    return 0;
}

QVariant QnUserListModel::data(const QModelIndex &index, int role) const {
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    QnUserResourcePtr user = d->userList[index.row()];

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case NameColumn:
            return user->getName();
        case PermissionsColumn:
            return d->permissionsString(user);
        default:
            break;
        } // switch (column)
        break;
    case Qt::ToolTipRole:
        switch (index.column()) {
        case NameColumn:
            return user->getName();
        case PermissionsColumn:
            return d->permissionsString(user);
        case LdapColumn:
            return user->isLdap() ? tr("LDAP user") : tr("Normal user");
        case EnabledColumn:
            return user->isEnabled() ? tr("Enabled") : tr("Disabled");
        case EditIconColumn:
            return tr("Edit user");
        default:
            break;
        } // switch (column)
        break;
    case Qt::DecorationRole:
        switch (index.column()) {
        case EditIconColumn:
            return qnSkin->icon("edit.png");
        case LdapColumn:
            if (user->isLdap())
                return qnSkin->icon("done.png");
            break;
        case EnabledColumn:
            if (user->isEnabled())
                return qnSkin->icon("done.png");
            break;
        default:
            break;
        }
        break;
    case Qt::ForegroundRole:
        /* Always use default color for checkboxes. */
        if (index.column() == CheckBoxColumn)
            return QVariant();
        /* Gray out disabled users. */
        if (!user->isEnabled()) {
            /* Highlighted users are brighter. */
            if (d->checkedUsers.contains(user))
                return d->colors.disabledSelectedText;
            return qApp->palette().color(QPalette::Disabled, QPalette::Text);
        }
        /* Highlight conflicting users. */
        if (user->isLdap() && !d->isUnique(user))
            return qnGlobals->errorTextColor();
        break;
    case Qt::BackgroundRole:
        if (d->checkedUsers.contains(user))
            return qApp->palette().color(QPalette::Highlight);
        break;
    case Qn::UserResourceRole:
        return QVariant::fromValue(user);
    case Qt::TextAlignmentRole:
        if (index.column() == LdapColumn)
            return Qt::AlignCenter;
        break;
    case Qt::CheckStateRole:
        if (index.column() == CheckBoxColumn)
            return d->checkedUsers.contains(user);
        break;
    default:
        break;
    } // switch (role)

    return QVariant();
}

QVariant QnUserListModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Vertical)
        return QVariant();

    if (section >= ColumnCount)
        return QVariant();

    if (role != Qt::DisplayRole)
        return base_type::headerData(section, orientation, role);

    switch (section) {
    case NameColumn:
        return tr("Name");
    case PermissionsColumn:
        return tr("Permissions");
    case LdapColumn:
        return tr("LDAP");
    case EnabledColumn:
        return tr("Enabled");
    default:
        return QString();
    }
}

Qt::ItemFlags QnUserListModel::flags(const QModelIndex &index) const {
    Qt::ItemFlags flags = Qt::NoItemFlags;

    QnUserResourcePtr user = d->user(index);
    if (!user)
        return flags;

    flags |= Qt::ItemIsSelectable | Qt::ItemIsEnabled;

    if (index.column() == CheckBoxColumn)
        flags |= Qt::ItemIsUserCheckable;

    return flags;
}

Qt::CheckState QnUserListModel::checkState() const {
    return d->checkState();
}

void QnUserListModel::setCheckState(Qt::CheckState state, const QnUserResourcePtr &user) {
    if (state == Qt::PartiallyChecked)
        return;

    auto roles = QVector<int>() << Qt::CheckStateRole << Qt::BackgroundRole << Qt::ForegroundRole;

    d->setCheckState(state, user);  
    if (!user) {
        emit dataChanged(index(0, CheckBoxColumn), index(d->userList.size() - 1, ColumnCount - 1), roles);
    }
    else {
        auto row = d->userIndex(user->getId());
        if (row >= 0)
            emit dataChanged(index(row, CheckBoxColumn), index(row, ColumnCount - 1), roles);
    }
        
}

const QnUserManagementColors QnUserListModel::colors() const {
    return d->colors;
}

void QnUserListModel::setColors(const QnUserManagementColors &colors) {
    beginResetModel();
    d->colors = colors;
    endResetModel();
}

QnSortedUserListModel::QnSortedUserListModel(QObject *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
{
}


bool QnSortedUserListModel::lessThan(const QModelIndex &left, const QModelIndex &right) const {
    QnUserResourcePtr leftUser = left.data(Qn::UserResourceRole).value<QnUserResourcePtr>();
    QnUserResourcePtr rightUser = right.data(Qn::UserResourceRole).value<QnUserResourcePtr>();

    Q_ASSERT(leftUser);
    Q_ASSERT(rightUser);

    if (!rightUser)
        return true;
    if (!leftUser)
        return false;

    switch (sortColumn()) {
    case QnUserListModel::PermissionsColumn: {
        qint64 leftPermissions = accessController()->globalPermissions(leftUser);
        qint64 rightPermissions = accessController()->globalPermissions(rightUser);
        if (leftPermissions == rightPermissions)
            break;
        return leftPermissions > rightPermissions; // Use ">" to make the owner higher than others
    }
    case QnUserListModel::EnabledColumn: {
        bool leftEnabled = leftUser->isEnabled();
        bool rightEnabled = rightUser->isEnabled();
        if (leftEnabled == rightEnabled)
            break;
        return leftEnabled;
    }
    case QnUserListModel::LdapColumn: {
        bool leftLdap = leftUser->isLdap();
        bool rightLdap = rightUser->isLdap();
        if (leftLdap == rightLdap)
            break;
        return leftLdap;
    }
    default:
        /* We should never sort by CheckBoxColumn. */
        break;
    }

    return naturalStringLess(leftUser->getName(), rightUser->getName());
}
