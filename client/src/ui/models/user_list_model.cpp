#include "user_list_model.h"

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <common/user_permissions.h>
#include <ui/style/skin.h>
#include <ui/style/globals.h>
#include <ui/workbench/workbench_access_controller.h>

class QnUserListModelPrivate : public QObject {
public:
    QnUserListModel *model;

    QnUserResourceList userList;

    QnUserListModelPrivate(QnUserListModel *parent)
        : QObject(parent)
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
};


void QnUserListModelPrivate::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if (!user)
        return;

    if (userIndex(user->getId()) != -1)
        return;

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

    model->beginRemoveRows(QModelIndex(), row, row);
    userList.removeAt(row);
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
    model->dataChanged(index, index.sibling(row, QnUserListModel::ColumnCount));
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
        permissionStrings.append(tr("Adjust camera settings"));
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

QnUserListModel::QnUserListModel(QObject *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
    , d(new QnUserListModelPrivate(this))
{
}

QnUserListModel::~QnUserListModel() {}

int QnUserListModel::rowCount(const QModelIndex &parent) const {
    return d->userList.size();
}

int QnUserListModel::columnCount(const QModelIndex &parent) const {
    return ColumnCount;
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
        case LdapColumn:
            if (user->isLdap()) {
                if (!user->isEnabled())
                    return tr("Disabled").toUpper();
            }
            break;
        default:
            break;
        } // switch (column)
        break;
    case Qt::DecorationRole:
        switch (index.column()) {
        case EditIconColumn:
            return qnSkin->icon("/edit.png");
        case LdapColumn:
            if (user->isLdap() && user->isEnabled())
                return qnSkin->icon("/done.png");
            break;
        }
        break;
    case Qt::ForegroundRole:
        if (user->isLdap()) {
            if (!user->isEnabled())
                return qApp->palette().color(QPalette::Disabled, QPalette::Text);
            if (!d->isUnique(user))
                return qnGlobals->errorTextColor();
        }
        break;
    case Qn::UserResourceRole:
        return QVariant::fromValue(user);
    case Qt::TextAlignmentRole:
        if (index.column() == LdapColumn)
            return Qt::AlignCenter;
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

    return flags;
}
