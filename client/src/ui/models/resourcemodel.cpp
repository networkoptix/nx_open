#include "resourcemodel.h"
#include "resourcemodel_p.h"

#ifndef USE_OLD_RESOURCEMODEL
#include <QtCore/QCoreApplication>
#include <QtCore/qtconcurrentrun.h>
#endif

#include <QtCore/QMimeData>
#include <QtCore/QUrl>

#include <core/resourcemanagment/resource_pool.h>

#include "ui/skin/skin.h"
#include "ui/view_drag_and_drop.h"

#include "file_processor.h"

enum IconKey {
    IconTypeUnknown = 0,
    IconTypeServer = 0x0001,
    IconTypeLayout = 0x0002,
    IconTypeCamera = 0x0004,
    IconTypeImage = 0x0008,
    IconTypeMedia = 0x0010,
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

#ifdef USE_OLD_RESOURCEMODEL
static inline QIcon iconForResource(quint32 m_flags, QnResource::Status m_status)
#else

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
#endif
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

    if ((m_flags & QnResource::local) == QnResource::local)
        key |= IconStateLocal;

    if (m_status == QnResource::Offline)
        key |= IconStateOffline;
    else if (m_status == QnResource::Unauthorized)
        key |= IconStateUnauthorized;

    return iconForKey(key);
}


ResourceModelPrivate::ResourceModelPrivate()
    : q_ptr(0)
{
}

ResourceModelPrivate::~ResourceModelPrivate()
{
#ifndef USE_OLD_RESOURCEMODEL
    Q_ASSERT(!nodes.values().contains(&root));
    qDeleteAll(nodes);
#endif
}

void ResourceModelPrivate::init()
{
    Q_Q(ResourceModel);

    QHash<int, QByteArray> roles = q->roleNames();
    roles.insert(Qt::UserRole + 1, "id");
    roles.insert(Qt::UserRole + 2, "searchString");
    q->setRoleNames(roles);

    q->connect(qnResPool, SIGNAL(resourceAdded(QnResourcePtr)), q, SLOT(_q_addResource(QnResourcePtr)));
    q->connect(qnResPool, SIGNAL(resourceRemoved(QnResourcePtr)), q, SLOT(_q_removeResource(QnResourcePtr)));
    q->connect(qnResPool, SIGNAL(resourceChanged(QnResourcePtr)), q, SLOT(_q_resourceChanged(QnResourcePtr)));

    const QnResourceList resources = qnResPool->getResources(); // make a snapshot
    foreach (const QnResourcePtr &server, resources) {
        if (server->checkFlag(QnResource::server)) {
            _q_addResource(server);
            const QnId serverId = server->getId();
            foreach (const QnResourcePtr &resource, resources) {
                if (resource->getParentId() == serverId && !resource->checkFlag(QnResource::server))
                    _q_addResource(resource);
            }
        }
    }
}

#ifndef USE_OLD_RESOURCEMODEL
Node *ResourceModelPrivate::node(const QModelIndex &index) const
{
    if (!index.isValid())
        return const_cast<Node *>(&root);

    Node *node = static_cast<Node *>(index.internalPointer());
    Q_ASSERT(node);
    return node;
}

Node *ResourceModelPrivate::node(QnId id) const
{
    if (id.isValid()) {
        if (Node *node = nodes.value(id, 0))
            return node;
    }
    return const_cast<Node *>(&root);
}

QModelIndex ResourceModelPrivate::index(int row, int column, const QModelIndex &parent) const
{
    Node *parentNode = this->node(parent);
    Q_ASSERT(parentNode);

    Node *node = nodeTree.value(parentNode).at(row);
    Q_ASSERT(node);
    return q_func()->createIndex(row, column, node);
}

QModelIndex ResourceModelPrivate::index(Node *node, int column) const
{
    if (!node || node == &root)
        return QModelIndex();

    Node *parentNode = this->node(node->parentId());
    Q_ASSERT(parentNode);

    const int row = nodeTree.value(parentNode).indexOf(node);
    Q_ASSERT(row >= 0);
    return q_func()->createIndex(row, column, node);
}

QModelIndex ResourceModelPrivate::index(QnId id, int column) const
{
    return index(node(id), column);
}

void ResourceModelPrivate::_q_addResource(const QnResourcePtr &resource)
{
    Q_Q(ResourceModel);

    Q_ASSERT(resource && resource->getId().isValid());

    Node *node = this->node(resource);
    if (node && node != &root)
        return; // avoid duplicates

    node = new Node(resource);
    const QModelIndex parentIndex = this->index(node->parentId());
    if ((node->flags() & QnResource::server) != QnResource::server && !parentIndex.isValid()) {
        qWarning("ResourceModel::addResource(): parent resource (id %d) wasn't found for resource (id %d)",
                 node->parentId(), node->id());
        delete node;
        return;
    }

    const int row = q->rowCount(parentIndex); // ### optimize for dynamic sort
    q->beginInsertRows(parentIndex, row, row);

    nodes[node->id()] = node;
    nodeTree[this->node(parentIndex)].insert(row, 1, node);

    q->endInsertRows();
}

void ResourceModelPrivate::_q_removeResource(const QnResourcePtr &resource)
{
    Q_Q(ResourceModel);

    Q_ASSERT(resource && resource->getId().isValid());

    Node *node = this->node(resource);
    if (!node || node == &root)
        return; // nothing to remove

    const QModelIndex parentIndex = this->index(node->parentId());

    const QModelIndex index = this->index(node);
    const int row = index.row();
    q->beginRemoveRows(parentIndex, row, row);

    nodes.remove(node->id());
    nodeTree[this->node(parentIndex)].remove(row, 1);

    q->endRemoveRows();
}

void ResourceModelPrivate::_q_resourceChanged(const QnResourcePtr &resource)
{
    Q_Q(ResourceModel);

    Q_ASSERT(resource && resource->getId().isValid());

    Node *node = this->node(resource);
    if (!node || node == &root)
        return; // nothing to change

    node->updateFromResource(resource);

    const QModelIndex index = this->index(node);
    Q_EMIT q->dataChanged(index, index);
}
#else
QModelIndex ResourceModelPrivate::index(QnId id, int column) const
{
    Q_Q(const ResourceModel);
    QModelIndex index;
    if (QStandardItem *item = q->item(0, 0)) {
        const QModelIndexList indexList = q->match(item->index(), Qt::UserRole + 1, id, 1, Qt::MatchExactly | Qt::MatchRecursive);
        index = indexList.value(0);
    }
    return index.isValid() ? index.sibling(index.row(), column) : index;
}

void ResourceModelPrivate::_q_addResource(const QnResourcePtr &resource)
{
    QStandardItem *item = this->item(resource);
    if (item)
        return; // avoid duplicates

    if (resource->checkFlag(QnResource::server)) {
        item = new QStandardItem;
        item->setSelectable(false);
        q_func()->appendRow(item);
    } else if (QStandardItem *root = this->item(resource->getParentId())) {
        item = new QStandardItem;
        root->appendRow(item);
    } else {
        qWarning("ResourceModel::addResource(): parent resource (id %d) wasn't found for resource (id %d)",
                 resource->getParentId(), resource->getId());
        return;
    }

    item->setText(resource->getName());
    item->setToolTip(resource->getName());
    item->setData(resource->getId(), Qt::UserRole + 1);
    item->setData(resource->toSearchString(), Qt::UserRole + 2);
    item->setIcon(iconForResource(resource->flags(), resource->getStatus()));
    item->setEditable(false);
}

void ResourceModelPrivate::_q_removeResource(const QnResourcePtr &resource)
{
    if (QStandardItem *item = this->item(resource)) {
        if (item->parent())
            item->parent()->removeRow(item->row());
        else
            q_func()->removeRow(item->row());
    }
}

void ResourceModelPrivate::_q_resourceChanged(const QnResourcePtr &resource)
{
    if (QStandardItem *item = this->item(resource)) {
        item->setText(resource->getName());
        item->setToolTip(resource->getName());
        item->setData(resource->getId(), Qt::UserRole + 1);
        item->setData(resource->toSearchString(), Qt::UserRole + 2);
        item->setIcon(iconForResource(resource->flags(), resource->getStatus()));
    }
}
#endif


ResourceModel::ResourceModel(QObject *parent)
#ifdef USE_OLD_RESOURCEMODEL
    : QStandardItemModel(parent), d_ptr(new ResourceModelPrivate)
#else
    : QAbstractItemModel(parent), d_ptr(new ResourceModelPrivate)
#endif
{
    Q_D(ResourceModel);
    d->q_ptr = this;
    d->init();

#ifndef USE_OLD_RESOURCEMODEL
    //refresh();
#endif
}

ResourceModel::~ResourceModel()
{
}

#ifdef USE_OLD_RESOURCEMODEL
QnResourcePtr ResourceModel::resource(const QModelIndex &index) const
{
    return index.column() == 0 ? qnResPool->getResourceById(index.data(Qt::UserRole + 1)) : QnResourcePtr(0);
}

QModelIndex ResourceModel::index(const QnResourcePtr &resource) const
{
    return d_func()->index(resource->getId());
}
#else
QnResourcePtr ResourceModel::resource(const QModelIndex &index) const
{
    Node *node = d_func()->node(index);
    return node ? node->resource() : QnResourcePtr(0);
}

QModelIndex ResourceModel::index(const QnResourcePtr &resource) const
{
    return d_func()->index(resource, 0);
}

/*!
    \reimp
*/
QModelIndex ResourceModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_D(const ResourceModel);
    if (row < 0 || column < 0 || row >= rowCount(parent) || column >= columnCount(parent))
        return QModelIndex();
    return d->index(row, column, parent);
}

/*!
    \reimp
*/
QModelIndex ResourceModel::buddy(const QModelIndex &index) const
{
    return index.sibling(index.row(), 0);
}

/*!
    \reimp
*/
QModelIndex ResourceModel::parent(const QModelIndex &index) const
{
    Q_D(const ResourceModel);
    Node *node = d->node(index);
    return d->index(node->parentId());
}

/*!
    \reimp
*/
bool ResourceModel::hasChildren(const QModelIndex &parent) const
{
    return rowCount(parent) > 0;
}

/*!
    \reimp
*/
int ResourceModel::rowCount(const QModelIndex &parent) const
{
    Q_D(const ResourceModel);
    if (parent.column() > 0)
        return 0;
    Node *node = d->node(parent);
    return d->nodeTree.value(node).size();
}

/*!
    \reimp
*/
int ResourceModel::columnCount(const QModelIndex &parent) const
{
    if (parent.model() == this || !parent.isValid())
        return 1;
    return 0;
}

/*!
    \reimp
*/
Qt::ItemFlags ResourceModel::flags(const QModelIndex &index) const
{
    Q_D(const ResourceModel);
    Qt::ItemFlags flags = Qt::NoItemFlags;
    if (d->isIndexValid(index)) {
        flags |= Qt::ItemIsEnabled | Qt::ItemIsDropEnabled;
        Node *node = d->node(index);
        if (node->parentId() != 0)
            flags |= Qt::ItemIsSelectable;
        if ((node->flags() & QnResource::local))
            flags |= Qt::ItemIsDragEnabled;
    }

    return flags;
}

/*!
    \reimp
*/
QVariant ResourceModel::data(const QModelIndex &index, int role) const
{
    Q_D(const ResourceModel);
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
    case Qt::UserRole + 1:
        return node->id();
    case Qt::UserRole + 2:
        return node->searchString();
    case Qt::UserRole + 3:
        return int(node->status());
    default:
        break;
    }

    return QVariant();
}

/*!
    \reimp
*/
bool ResourceModel::setData(const QModelIndex &index, const QVariant &value, int role)
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

/*!
    \reimp
*/
QVariant ResourceModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        if (role == Qt::DisplayRole) {
            // ###
        }

        return section + 1;
    }

    return QVariant();
}
#endif

void ResourceModel::addResource(const QnResourcePtr &resource)
{
    if (sender() != qnResPool) {
        qnResPool->addResource(resource); // emits resourceAdded; thus return
        return;
    }

    d_func()->_q_addResource(resource);
}

void ResourceModel::removeResource(const QnResourcePtr &resource)
{
    if (sender() != qnResPool) {
        qnResPool->removeResource(resource); // emits resourceRemoved; thus return
        return;
    }

    d_func()->_q_removeResource(resource);
}

/*!
    \reimp
*/
QStringList ResourceModel::mimeTypes() const
{
#ifdef USE_OLD_RESOURCEMODEL
    QStringList mimeTypes = QStandardItemModel::mimeTypes();
#else
    QStringList mimeTypes = QAbstractItemModel::mimeTypes();
#endif
    mimeTypes.append(QLatin1String("text/uri-list"));
    mimeTypes.append(resourcesMime());

    return mimeTypes;
}

/*!
    \reimp
*/
QMimeData *ResourceModel::mimeData(const QModelIndexList &indexes) const
{
#ifdef USE_OLD_RESOURCEMODEL
    QMimeData *mimeData = QStandardItemModel::mimeData(indexes);
#else
    QMimeData *mimeData = QAbstractItemModel::mimeData(indexes);
#endif
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

/*!
    \reimp
*/
bool ResourceModel::dropMimeData(const QMimeData *mimeData, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
    // check if the action is supported
    if (!mimeData || !(action == Qt::CopyAction || action == Qt::MoveAction))
        return false;

    // check if the format is supported
    const QString format = resourcesMime();
    if (!mimeData->hasFormat(format) && !mimeData->hasUrls())
#ifdef USE_OLD_RESOURCEMODEL
        return QStandardItemModel::dropMimeData(mimeData, action, row, column, parent);
#else
        return QAbstractItemModel::dropMimeData(mimeData, action, row, column, parent);
#endif

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

/*!
    \reimp
*/
Qt::DropActions ResourceModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}


ResourceSortFilterProxyModelPrivate::ResourceSortFilterProxyModelPrivate()
    : q_ptr(0)
{
}

bool ResourceSortFilterProxyModelPrivate::matchesFilters(const QRegExp filters[], Node *node, int source_row, const QModelIndex &source_parent) const
{
    Q_Q(const ResourceSortFilterProxyModel);

    if (!filters[Id].isEmpty()) {
        const QString id = QString::number(node->id());
        if (filters[Id].exactMatch(id))
            return true;
    }

    if (!filters[Name].isEmpty()) {
        const QString name = node->name();
        if (filters[Name].exactMatch(name))
            return true;
    }

    if (!filters[Tags].isEmpty()) {
        QString tags = node->resource()->tagList().join(QLatin1String("\",\""));
        if (!tags.isEmpty())
            tags = QLatin1Char('"') + tags + QLatin1Char('"');
        if (filters[Tags].exactMatch(tags))
            return true;
    }

    if (!filters[Text].isEmpty()) {
        Q_ASSERT(source_parent.isValid()); // checked in filterAcceptsRow()
        const int filter_role = q->filterRole();
        const int filter_column = q->filterKeyColumn();
        if (filter_column == -1) {
            const int columnCount = source_parent.model()->columnCount(source_parent);
            for (int column = 0; column < columnCount; ++column) {
                const QModelIndex source_index = source_parent.child(source_row, column);
                const QString text = source_index.data(filter_role).toString();
                if (filters[Text].exactMatch(text))
                    return true;
            }
        } else {
            const QModelIndex source_index = source_parent.child(source_row, filter_column);
            if (source_index.isValid()) { // the column may not exist
                const QString text = source_index.data(filter_role).toString();
                if (filters[Text].exactMatch(text))
                    return true;
            }
        }
    }

    return false;
}

QString ResourceSortFilterProxyModelPrivate::normalizedFilterString(const QString &str)
{
    QString ret = str;
    ret.replace(QLatin1Char('"'), QString());
    return ret;
}

void ResourceSortFilterProxyModelPrivate::buildFilters(const QSet<QString> parts[], QRegExp *filters)
{
    for (uint i = 0; i < NumFilterCategories; ++i) {
        if (!parts[i].isEmpty()) {
            QString pattern;
            foreach (const QString &part, parts[i]) {
                if (!pattern.isEmpty())
                    pattern += QLatin1Char('|');
                pattern += QRegExp::escape(part);
            }
            pattern = QLatin1Char('(') + pattern + QLatin1Char(')');
            if (i < Id) {
                pattern = QLatin1String(".*") + pattern + QLatin1String(".*");
                filters[i] = QRegExp(pattern, Qt::CaseInsensitive, QRegExp::RegExp2);
            } else {
                filters[i] = QRegExp(pattern, Qt::CaseSensitive, QRegExp::RegExp2);
            }
        }
    }
}

void ResourceSortFilterProxyModelPrivate::parseFilterString()
{
    Q_Q(ResourceSortFilterProxyModel);

    flagsFilter = 0;
    for (uint i = 0; i < NumFilterCategories; ++i) {
         // ### don't invalidate filters which weren't changed
        filters[i] = QRegExp();
        negfilters[i] = QRegExp();
    }

    parsedFilterString = q->filterRegExp().pattern();
    if (parsedFilterString.isEmpty())
        return;

    const QRegExp::PatternSyntax patternSyntax = q->filterRegExp().patternSyntax();
    if (patternSyntax != QRegExp::FixedString && patternSyntax != QRegExp::Wildcard && patternSyntax != QRegExp::WildcardUnix)
        return;

    QSet<QString> filters_[NumFilterCategories];
    QSet<QString> negfilters_[NumFilterCategories];

    const QString filterString = normalizedFilterString(parsedFilterString);
    QRegExp rx(QLatin1String("(\\W?)(\\w+:)?(\\w*)"), Qt::CaseSensitive, QRegExp::RegExp2);
    int pos = 0;
    while ((pos = rx.indexIn(filterString, pos)) != -1) {
        pos += rx.matchedLength();
        if (rx.matchedLength() == 0)
            break;

        const QString sign = rx.cap(1);
        const QString key = rx.cap(2).toLower();
        const QString pattern = rx.cap(3);
        if (pattern.isEmpty())
            continue;

        FilterCategory category = Text;
        if (!key.isEmpty()) {
            if (key == QLatin1String("id:"))
                category = Id;
            else if (key == QLatin1String("name:"))
                category = Name;
            else if (key == QLatin1String("tag:"))
                category = Tags;
        } else {
            if (pattern == QLatin1String("live"))
                flagsFilter |= QnResource::live;
        }
        if (sign == QLatin1String("-"))
            negfilters_[category].insert(pattern);
        else
            filters_[category].insert(pattern);
    }

    buildFilters(filters_, filters);
    buildFilters(negfilters_, negfilters);
}


ResourceSortFilterProxyModel::ResourceSortFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent), d_ptr(new ResourceSortFilterProxyModelPrivate)
{
    Q_D(ResourceSortFilterProxyModel);
    d->q_ptr = this;
    d->parseFilterString();
}

QnResourcePtr ResourceSortFilterProxyModel::resourceFromIndex(const QModelIndex &index) const
{
    ResourceModel *resourceModel = qobject_cast<ResourceModel *>(sourceModel());
    return resourceModel ? resourceModel->resource(mapToSource(index)) : QnResourcePtr(0);
}

QModelIndex ResourceSortFilterProxyModel::indexFromResource(const QnResourcePtr &resource) const
{
    ResourceModel *resourceModel = qobject_cast<ResourceModel *>(sourceModel());
    return resourceModel ? mapFromSource(resourceModel->index(resource)) : QModelIndex();
}

bool ResourceSortFilterProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    Q_D(const ResourceSortFilterProxyModel);

    ResourceModel *resourceModel = qobject_cast<ResourceModel *>(sourceModel());
    if (!resourceModel)
        return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);

    if (!source_parent.isValid())
        return true; // include root nodes

    if (d->parsedFilterString != filterRegExp().pattern())
        const_cast<ResourceSortFilterProxyModelPrivate *>(d)->parseFilterString();
    if (d->parsedFilterString.isEmpty())
        return false;

    Node *node = resourceModel->d_func()->node(resourceModel->index(source_row, 0, source_parent));
    if (!node || node->id() == 0)
        return false;

    if (d->matchesFilters(d->negfilters, node, source_row, source_parent))
        return false;

    if (d->flagsFilter != 0) {
        if ((node->flags() & d->flagsFilter) != 0)
            return true;
    }

    if (d->matchesFilters(d->filters, node, source_row, source_parent))
        return true;

    return false;
}

#include "moc_resourcemodel.cpp"
