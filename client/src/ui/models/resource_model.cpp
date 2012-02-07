#include "resource_model.h"
#include "resource_model_p.h"

#include <QtCore/QCoreApplication>
#include <QtCore/qtconcurrentrun.h>
#include <QtCore/QMimeData>
#include <QtCore/QUrl>

#include <core/resourcemanagment/resource_pool.h>

#include "ui/style/skin.h"
#include "ui/view_drag_and_drop.h"

#include "file_processor.h"

enum IconKey {
    IconTypeUnknown = 0,
    IconTypeServer = 0x0001,
    IconTypeLayout = 0x0002,
    IconTypeCamera = 0x0004,
    IconTypeImage = 0x0008,
    IconTypeMedia = 0x0010,
    IconTypeUser = 0x0020,
    IconTypeMask = 0x0fff,

    IconStateLocal = 0x1000,
    IconStateOffline = 0x2000,
    IconStateUnauthorized = 0x4000,
    IconStateMask = 0xf000
};

typedef QHash<quint32, QIcon> IconCache;
Q_GLOBAL_STATIC(IconCache, iconCache)

static void invalidateIconCache()
{
    IconCache *cache = iconCache();
    if (!cache)
        return;

    cache->clear();

    cache->insert(IconTypeUnknown, QIcon());
    cache->insert(IconTypeServer | IconStateLocal, Skin::icon(QLatin1String("home.png")));
    cache->insert(IconTypeServer, Skin::icon(QLatin1String("server.png")));
    cache->insert(IconTypeLayout, Skin::icon(QLatin1String("layout.png")));
    cache->insert(IconTypeCamera, Skin::icon(QLatin1String("webcam.png")));
    cache->insert(IconTypeImage, Skin::icon(QLatin1String("snapshot.png")));
    cache->insert(IconTypeMedia, Skin::icon(QLatin1String("media.png")));
    cache->insert(IconTypeUser, Skin::icon(QLatin1String("unauthorized.png")));

    cache->insert(IconStateOffline, Skin::icon(QLatin1String("offline.png")));
    cache->insert(IconStateUnauthorized, Skin::icon(QLatin1String("unauthorized.png")));
}

static QIcon iconForKey(quint32 key)
{
    IconCache *cache = iconCache();
    if (!cache)
        return QIcon();

    if (cache->isEmpty())
        invalidateIconCache();

    if ((key & IconTypeMask) == IconTypeUnknown)
        key = IconTypeUnknown;

    if (!cache->contains(key)) {
        QIcon icon;
        quint32 state;
        if ((key & IconStateMask) != 0) {
            for (state = IconStateUnauthorized; state >= IconStateLocal; state >>= 1) {
                if ((key & state) != 0) {
                    icon = iconForKey(key & ~state);
                    break;
                }
            }
        }
        if (!icon.isNull()) {
            QIcon overlayIcon;
            if (cache->contains(state))
                overlayIcon = cache->value(state);
            if (!overlayIcon.isNull()) {
                QPixmap pixmap = icon.pixmap(QSize(256, 256));
                {
                    QPainter painter(&pixmap);
                    QRect r = pixmap.rect();
                    // ### allow overlay combinations
                    r.setTopLeft(r.center());
                    overlayIcon.paint(&painter, r, Qt::AlignRight | Qt::AlignBottom);
                }
                icon = QIcon(pixmap);
            }
        }
        cache->insert(key, icon);
    }

    return cache->value(key);
}


Node::Node(const QnResourcePtr &resource)
{
    m_id = resource->getId();
    m_parentId = resource->getParentId();
    m_flags = resource->flags();
    m_status = resource->getStatus();
    m_name = resource->getName();
    m_searchString = resource->toSearchString();
}

void Node::updateFromResource(const QnResourcePtr &resource)
{
    m_status = resource->getStatus();
    m_name = resource->getName();
    m_searchString = resource->toSearchString();
}

QnResourcePtr Node::resource() const
{
    return qnResPool->getResourceById(m_id);
}

QIcon Node::icon() const
{
    quint32 key = IconTypeUnknown;

    if ((m_flags & QnResource::server) == QnResource::server)
        key |= IconTypeServer;
    else if ((m_flags & QnResource::layout) == QnResource::layout)
        key |= IconTypeLayout;
    else if ((m_flags & QnResource::live_cam) == QnResource::live_cam)
        key |= IconTypeCamera;
    else if ((m_flags & QnResource::SINGLE_SHOT) == QnResource::SINGLE_SHOT)
        key |= IconTypeImage;
    else if ((m_flags & QnResource::ARCHIVE) == QnResource::ARCHIVE)
        key |= IconTypeMedia;
    else if ((m_flags & QnResource::server_archive) == QnResource::server_archive)
        key |= IconTypeMedia;
    else if ((m_flags & QnResource::user) == QnResource::user) {
        key |= IconTypeUser;
    }

    if ((m_flags & QnResource::local) == QnResource::local)
        key |= IconStateLocal;

    if (m_status == QnResource::Offline)
        key |= IconStateOffline;
    else if (m_status == QnResource::Unauthorized)
        key |= IconStateUnauthorized;

    return iconForKey(key);
}


QnResourceModelPrivate::QnResourceModelPrivate()
    : q_ptr(0)
{
}

QnResourceModelPrivate::~QnResourceModelPrivate()
{
    Q_ASSERT(!nodes.values().contains(&root));
    qDeleteAll(nodes);
}

void QnResourceModelPrivate::init()
{
    Q_Q(QnResourceModel);

    QHash<int, QByteArray> roles = q->roleNames();
    roles.insert(QnResourceModel::ResourceRole,     "resource");
    roles.insert(QnResourceModel::IdRole,           "id");
    roles.insert(QnResourceModel::SearchStringRole, "searchString");
    roles.insert(QnResourceModel::StatusRole,       "status");
    q->setRoleNames(roles);

    q->connect(qnResPool, SIGNAL(resourceAdded(QnResourcePtr)), q, SLOT(at_resPool_resourceAdded(QnResourcePtr)));
    q->connect(qnResPool, SIGNAL(resourceRemoved(QnResourcePtr)), q, SLOT(at_resPool_resourceRemoved(QnResourcePtr)));
    q->connect(qnResPool, SIGNAL(resourceChanged(QnResourcePtr)), q, SLOT(at_resPool_resourceChanged(QnResourcePtr)));

    const QnResourceList resources = qnResPool->getResources(); // make a snapshot
    foreach (const QnResourcePtr &server, resources) {
        if (server->checkFlag(QnResource::server)) {
            at_resPool_resourceAdded(server);
            const QnId serverId = server->getId();
            foreach (const QnResourcePtr &resource, resources) {
                if (resource->getParentId() == serverId && !resource->checkFlag(QnResource::server))
                    at_resPool_resourceAdded(resource);
            }
        }
    }
}

void QnResourceModelPrivate::insertNode(Node *parentNode, Node *node, int row)
{
    nodes[node->id()] = node;
    nodeTree[parentNode].insert(row, 1, node);
}

void QnResourceModelPrivate::removeNodes(Node *parentNode, int row, int count)
{
    for (int i = row + count - 1; i >= row; --i) {
        Node *node = nodeTree[parentNode].at(i);
        removeNodes(node, 0, nodeTree[node].size());
        nodeTree.remove(node);
        nodes.remove(node->id());
        delete node;
    }
    nodeTree[parentNode].remove(row, count);
}

Node *QnResourceModelPrivate::node(const QModelIndex &index) const
{
    if (!index.isValid())
        return const_cast<Node *>(&root);

    Node *node = static_cast<Node *>(index.internalPointer());
    Q_ASSERT(node);
    return node;
}

Node *QnResourceModelPrivate::node(QnId id) const
{
    if (id.isValid()) {
        if (Node *node = nodes.value(id, 0))
            return node;
    }
    return const_cast<Node *>(&root);
}

QModelIndex QnResourceModelPrivate::index(int row, int column, const QModelIndex &parent) const
{
    Node *parentNode = this->node(parent);
    Q_ASSERT(parentNode);

    Node *node = nodeTree.value(parentNode).at(row);
    Q_ASSERT(node);
    return q_func()->createIndex(row, column, node);
}

QModelIndex QnResourceModelPrivate::index(Node *node, int column) const
{
    if (!node || node == &root)
        return QModelIndex();

    Node *parentNode = this->node(node->parentId());
    Q_ASSERT(parentNode);

    const int row = nodeTree.value(parentNode).indexOf(node);
    Q_ASSERT(row >= 0);
    return q_func()->createIndex(row, column, node);
}

QModelIndex QnResourceModelPrivate::index(QnId id, int column) const
{
    return index(node(id), column);
}

void QnResourceModelPrivate::at_resPool_resourceAdded(const QnResourcePtr &resource)
{
    Q_Q(QnResourceModel);

    Q_ASSERT(resource && resource->getId().isValid());

    Node *node = this->node(resource);
    if (node && node != &root)
        return; // avoid duplicates

    node = new Node(resource);
    const QModelIndex parentIndex = this->index(node->parentId());
    if ((node->flags() & QnResource::server) != QnResource::server && !parentIndex.isValid()) {
        qWarning("QnResourceModel::addResource(): parent resource (id %d) wasn't found for resource (id %d)",
                 node->parentId(), node->id());
        delete node;
        return;
    }

    const int row = q->rowCount(parentIndex); // ### optimize for dynamic sort
    q->beginInsertRows(parentIndex, row, row);

    insertNode(this->node(parentIndex), node, row);

    q->endInsertRows();
}

void QnResourceModelPrivate::at_resPool_resourceRemoved(const QnResourcePtr &resource)
{
    Q_Q(QnResourceModel);

    Q_ASSERT(resource && resource->getId().isValid());

    Node *node = this->node(resource);
    if (!node || node == &root)
        return; // nothing to remove

    const QModelIndex parentIndex = this->index(node->parentId());

    const QModelIndex index = this->index(node);
    const int row = index.row();
    q->beginRemoveRows(parentIndex, row, row);

    removeNodes(this->node(parentIndex), row, 1);

    q->endRemoveRows();
}

void QnResourceModelPrivate::at_resPool_resourceChanged(const QnResourcePtr &resource)
{
    Q_Q(QnResourceModel);

    Q_ASSERT(resource && resource->getId().isValid());

    Node *node = this->node(resource);
    if (!node || node == &root)
        return; // nothing to change

    node->updateFromResource(resource);

    const QModelIndex index = this->index(node);
    Q_EMIT q->dataChanged(index, index);
}

QnResourceModel::QnResourceModel(QObject *parent)
    : QAbstractItemModel(parent), d_ptr(new QnResourceModelPrivate)
{
    Q_D(QnResourceModel);
    d->q_ptr = this;
    d->init();
}

QnResourceModel::~QnResourceModel()
{
}

QModelIndex QnResourceModel::index(const QnResourcePtr &resource) const
{
    return d_func()->index(resource, 0);
}

QModelIndex QnResourceModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_D(const QnResourceModel);
    if (row < 0 || column < 0 || row >= rowCount(parent) || column >= columnCount(parent))
        return QModelIndex();
    return d->index(row, column, parent);
}

QModelIndex QnResourceModel::buddy(const QModelIndex &index) const
{
    return index.sibling(index.row(), 0);
}

QModelIndex QnResourceModel::parent(const QModelIndex &index) const
{
    Q_D(const QnResourceModel);
    Node *node = d->node(index);
    return d->index(node->parentId());
}

bool QnResourceModel::hasChildren(const QModelIndex &parent) const
{
    return rowCount(parent) > 0;
}

int QnResourceModel::rowCount(const QModelIndex &parent) const
{
    Q_D(const QnResourceModel);
    if (parent.column() > 0)
        return 0;
    Node *node = d->node(parent);
    return d->nodeTree.value(node).size();
}

int QnResourceModel::columnCount(const QModelIndex &parent) const
{
    if (parent.model() == this || !parent.isValid())
        return 1;
    return 0;
}

Qt::ItemFlags QnResourceModel::flags(const QModelIndex &index) const
{
    Q_D(const QnResourceModel);
    Qt::ItemFlags flags = Qt::NoItemFlags;
    if (d->isIndexValid(index)) {
        flags |= Qt::ItemIsEnabled | Qt::ItemIsDropEnabled;
        Node *node = d->node(index);
        flags |= Qt::ItemIsSelectable;
        if ((node->flags() & QnResource::media))
            flags |= Qt::ItemIsDragEnabled;
    }

    return flags;
}

QVariant QnResourceModel::data(const QModelIndex &index, int role) const
{
    Q_D(const QnResourceModel);
    if (!d->isIndexValid(index))
        return QVariant();

    Node *node = d->node(index);
    switch (role) {
    case Qt::DisplayRole:
        return node->name();
    case Qt::DecorationRole:
        if (index.column() == 0)
            return node->icon();
        break;
    case Qt::EditRole:
        return node->id();
    case Qt::ToolTipRole: // ###
    case Qt::StatusTipRole: // ###
    case Qt::WhatsThisRole: // ###
        return node->name();
    case Qt::FontRole:
        // ###
        break;
    case Qt::TextAlignmentRole:
        // ###
        break;
    case Qt::BackgroundRole:
        // ###
        break;
    case Qt::ForegroundRole:
        // ###
        break;
    case Qt::CheckStateRole:
        // ###
        break;
    case Qt::AccessibleTextRole: // ###
    case Qt::AccessibleDescriptionRole: // ###
        return node->name();
    case ResourceRole:
        return QVariant::fromValue<QnResourcePtr>(node->resource());
    case IdRole: //Qt::UserRole + 1:
        return node->id();
    case SearchStringRole: //Qt::UserRole + 2:
        return node->searchString();
    case StatusRole: //Qt::UserRole + 3:
        return int(node->status());
    default:
        break;
    }

    return QVariant();
}

bool QnResourceModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
/* if (role != Qt::EditRole)
         return false;

    TreeItem *item = getItem(index);
    bool result = item->setData(index.column(), value);

    if (result)
         Q_EMIT dataChanged(index, index);

    return result;*/
    return false;
}

QVariant QnResourceModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        if (role == Qt::DisplayRole) {
            // ###
        }

        return section + 1;
    }

    return QVariant();
}

void QnResourceModel::addResource(const QnResourcePtr &resource)
{
    if (sender() != qnResPool) {
        qnResPool->addResource(resource); // emits resourceAdded; thus return
        return;
    }

    d_func()->at_resPool_resourceAdded(resource);
}

void QnResourceModel::removeResource(const QnResourcePtr &resource)
{
    if (sender() != qnResPool) {
        qnResPool->removeResource(resource); // emits resourceRemoved; thus return
        return;
    }

    d_func()->at_resPool_resourceRemoved(resource);
}

QStringList QnResourceModel::mimeTypes() const
{
    QStringList mimeTypes = QAbstractItemModel::mimeTypes();
    mimeTypes.append(QLatin1String("text/uri-list"));
    mimeTypes.append(resourcesMime());

    return mimeTypes;
}

QMimeData *QnResourceModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = QAbstractItemModel::mimeData(indexes);
    if (mimeData) {
        const QStringList types = mimeTypes();
        const QString resourceFormat = resourcesMime();
        const QString urlFormat = QLatin1String("text/uri-list");
        if (types.contains(resourceFormat) || types.contains(urlFormat)) {
            QnResourceList resources;
            foreach (const QModelIndex &index, indexes) {
                if (QnResourcePtr resource = this->resource(index))
                    resources.append(resource);
            }
            if (types.contains(resourceFormat))
                mimeData->setData(resourceFormat, serializeResources(resources));
            if (types.contains(urlFormat)) {
                QList<QUrl> urls;
                foreach (const QnResourcePtr &resource, resources) {
                    if (resource->checkFlag(QnResource::url))
                        urls.append(QUrl::fromLocalFile(resource->getUrl()));
                }
                mimeData->setUrls(urls);
            }
        }
    }

    return mimeData;
}

bool QnResourceModel::dropMimeData(const QMimeData *mimeData, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
    // check if the action is supported
    if (!mimeData || !(action == Qt::CopyAction || action == Qt::MoveAction))
        return false;

    // check if the format is supported
    const QString format = resourcesMime();
    if (!mimeData->hasFormat(format) && !mimeData->hasUrls())
        return QAbstractItemModel::dropMimeData(mimeData, action, row, column, parent);

    // decode and insert
    QnResourceList resources;
    if (!mimeData->hasFormat(format))
        resources += deserializeResources(mimeData->data(format));
    else if (mimeData->hasUrls())
        resources += QnFileProcessor::createResourcesForFiles(QnFileProcessor::findAcceptedFiles(mimeData->urls()));
    foreach (const QnResourcePtr &resource, resources) {
        if (resource->checkFlag(QnResource::local) || qnResPool->getResourceById(resource->getParentId()))
            qnResPool->addResource(resource);
    }

    return true;
}

Qt::DropActions QnResourceModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

void QnResourceModel::at_resPool_resourceAdded(const QnResourcePtr &resource) {
    d_func()->at_resPool_resourceAdded(resource);
}

void QnResourceModel::at_resPool_resourceRemoved(const QnResourcePtr &resource) {
    d_func()->at_resPool_resourceRemoved(resource);
}

void QnResourceModel::at_resPool_resourceChanged(const QnResourcePtr &resource) {
    d_func()->at_resPool_resourceChanged(resource);
}


