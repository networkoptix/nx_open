#include "resourcemodel.h"
#include "resourcemodel_p.h"

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

static inline QIcon iconForResource(const QnResourcePtr &resource)
{
    quint32 key = IconTypeUnknown;

    const quint32 flags = resource->flags();
    if ((flags & QnResource::server) == QnResource::server)
        key |= IconTypeServer;
    else if ((flags & QnResource::layout) == QnResource::layout)
        key |= IconTypeLayout;
    else if ((flags & QnResource::live_cam) == QnResource::live_cam)
        key |= IconTypeCamera;
    else if ((flags & QnResource::SINGLE_SHOT) == QnResource::SINGLE_SHOT)
        key |= IconTypeImage;
    else if ((flags & QnResource::ARCHIVE) == QnResource::ARCHIVE)
        key |= IconTypeMedia;
    else if ((flags & QnResource::server_archive) == QnResource::server_archive)
        key |= IconTypeMedia;

    if ((flags & QnResource::local) == QnResource::local)
        key |= IconStateLocal;

    const QnResource::Status status = resource->getStatus();
    if (status == QnResource::Offline)
        key |= IconStateOffline;
    else if (status == QnResource::Unauthorized)
        key |= IconStateUnauthorized;

    return iconForKey(key);
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

void ResourceModelPrivate::_q_addResource(const QnResourcePtr &resource)
{
    QStandardItem *item = itemFromResource(resource);
    if (item)
        return; // avoid duplicates

    if (resource->checkFlag(QnResource::server)) {
        item = new QStandardItem;
        item->setSelectable(false);
        q_func()->appendRow(item);
    } else if (QStandardItem *root = itemFromResourceId(resource->getParentId().hash())) {
        item = new QStandardItem;
        root->appendRow(item);
    } else {
        qWarning("ResourceModel::addResource(): parent resource (id %d) wasn't found for resource (id %d)",
                 resource->getParentId().hash(), resource->getId().hash());
        return;
    }

    item->setText(resource->getName());
    item->setToolTip(resource->getName());
    item->setData(resource->getId().hash(), Qt::UserRole + 1);
    item->setData(resource->toSearchString(), Qt::UserRole + 2);
    item->setIcon(iconForResource(resource));
    item->setEditable(false);
}

void ResourceModelPrivate::_q_removeResource(const QnResourcePtr &resource)
{
    if (QStandardItem *item = itemFromResource(resource)) {
        if (item->parent())
            item->parent()->removeRow(item->row());
        else
            q_func()->removeRow(item->row());
    }
}

void ResourceModelPrivate::_q_resourceChanged(const QnResourcePtr &resource)
{
    if (QStandardItem *item = itemFromResource(resource)) {
        item->setText(resource->getName());
        item->setToolTip(resource->getName());
        item->setData(resource->getId().hash(), Qt::UserRole + 1);
        item->setData(resource->toSearchString(), Qt::UserRole + 2);
        item->setIcon(iconForResource(resource));
    }
}


ResourceModel::ResourceModel(QObject *parent)
    : QStandardItemModel(parent), d_ptr(new ResourceModelPrivate)
{
    Q_D(ResourceModel);

    d->q_ptr = this;
    d->init();
}

ResourceModel::~ResourceModel()
{
}

QnResourcePtr ResourceModel::resourceFromIndex(const QModelIndex &index) const
{
    return index.column() == 0 ? qnResPool->getResourceById(index.data(Qt::UserRole + 1)) : QnResourcePtr(0);
}

QModelIndex ResourceModel::indexFromResource(const QnResourcePtr &resource) const
{
    return indexFromResourceId(resource->getId().hash());
}

QModelIndex ResourceModel::indexFromResourceId(uint id) const // ### remove; use use indexFromResource() instead
{
    const QModelIndexList indexList = match(index(0, 0), Qt::UserRole + 1, id, 1, Qt::MatchExactly | Qt::MatchRecursive);
    return indexList.value(0);
}

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

QStringList ResourceModel::mimeTypes() const
{
    QStringList mimeTypes = QStandardItemModel::mimeTypes();
    mimeTypes.append(QLatin1String("text/uri-list"));
    mimeTypes.append(resourcesMime());

    return mimeTypes;
}

QMimeData *ResourceModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = QStandardItemModel::mimeData(indexes);
    if (mimeData) {
        const QStringList types = mimeTypes();
        const QString resourceFormat = resourcesMime();
        const QString urlFormat = QLatin1String("text/uri-list");
        if (types.contains(resourceFormat) || types.contains(urlFormat)) {
            QnResourceList resources;
            foreach (const QModelIndex &index, indexes) {
                if (QnResourcePtr resource = resourceFromIndex(index))
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

bool ResourceModel::dropMimeData(const QMimeData *mimeData, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
    // check if the action is supported
    if (!mimeData || !(action == Qt::CopyAction || action == Qt::MoveAction))
        return false;

    // check if the format is supported
    const QString format = resourcesMime();
    if (!mimeData->hasFormat(format) && !mimeData->hasUrls())
        return QStandardItemModel::dropMimeData(mimeData, action, row, column, parent);

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


ResourceSortFilterProxyModel::ResourceSortFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    parseFilterString();
}

QnResourcePtr ResourceSortFilterProxyModel::resourceFromIndex(const QModelIndex &index) const
{
    return index.column() == 0 ? qnResPool->getResourceById(index.data(Qt::UserRole + 1)) : QnResourcePtr(0);
}

QModelIndex ResourceSortFilterProxyModel::indexFromResource(const QnResourcePtr &resource) const
{
    const QModelIndexList indexList = match(index(0, 0), Qt::UserRole + 1, resource->getId().hash(), 1, Qt::MatchExactly | Qt::MatchRecursive);
    return indexList.value(0);
}

bool ResourceSortFilterProxyModel::matchesFilters(const QRegExp filters[], const QnResourcePtr &resource,
                                                  QAbstractItemModel *sourceModel, int source_row, const QModelIndex &source_parent) const
{
    if (!filters[Id].isEmpty()) {
        const QString id = QString::number(resource->getId().hash());
        if (filters[Id].exactMatch(id))
            return true;
    }

    if (!filters[Name].isEmpty()) {
        const QString name = resource->getName();
        if (filters[Name].exactMatch(name))
            return true;
    }

    if (!filters[Tags].isEmpty()) {
        QString tags = resource->tagList().join(QLatin1String("\",\""));
        if (!tags.isEmpty())
            tags = QLatin1Char('"') + tags + QLatin1Char('"');
        if (filters[Tags].exactMatch(tags))
            return true;
    }

    if (!filters[Text].isEmpty()) {
        const int filter_role = filterRole();
        const int filter_column = filterKeyColumn();
        if (filter_column == -1) {
            const int columnCount = sourceModel->columnCount(source_parent);
            for (int column = 0; column < columnCount; ++column) {
                const QModelIndex source_index = sourceModel->index(source_row, column, source_parent);
                const QString text = source_index.data(filter_role).toString();
                if (filters[Text].exactMatch(text))
                    return true;
            }
        } else {
            const QModelIndex source_index = sourceModel->index(source_row, filter_column, source_parent);
            if (source_index.isValid()) { // the column may not exist
                const QString text = source_index.data(filter_role).toString();
                if (filters[Text].exactMatch(text))
                    return true;
            }
        }
    }

    return false;
}

bool ResourceSortFilterProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    if (!source_parent.isValid())
        return true; // include root nodes

    if (m_parsedFilterString != filterRegExp().pattern())
        const_cast<ResourceSortFilterProxyModel *>(this)->parseFilterString();
    if (m_parsedFilterString.isEmpty())
        return false;

    QnResourcePtr resource = resourceFromIndex(sourceModel()->index(source_row, 0, source_parent));
    if (!resource)
        return false;

    if (matchesFilters(m_negfilters, resource, sourceModel(), source_row, source_parent))
        return false;

    if (m_flagsFilter != 0) {
        if (resource->flags() & m_flagsFilter)
            return true;
    }

    if (matchesFilters(m_filters, resource, sourceModel(), source_row, source_parent))
        return true;

    return false;
}

static inline QString normalizedFilterString(const QString &str)
{
    QString ret = str;
    ret.replace(QLatin1Char('"'), QString());
    return ret;
}

void ResourceSortFilterProxyModel::parseFilterString()
{
    m_flagsFilter = 0;
    for (uint i = 0; i < NumFilterCategories; ++i) {
         // ### don't invalidate filters which weren't changed
        m_filters[i] = QRegExp();
        m_negfilters[i] = QRegExp();
    }

    m_parsedFilterString = filterRegExp().pattern();
    if (m_parsedFilterString.isEmpty())
        return;

    const QRegExp::PatternSyntax patternSyntax = filterRegExp().patternSyntax();
    if (patternSyntax != QRegExp::FixedString && patternSyntax != QRegExp::Wildcard && patternSyntax != QRegExp::WildcardUnix)
        return;

    QSet<QString> filters[NumFilterCategories];
    QSet<QString> negfilters[NumFilterCategories];

    const QString filterString = normalizedFilterString(m_parsedFilterString);
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
                m_flagsFilter |= QnResource::live;
        }
        if (sign == QLatin1String("-"))
            negfilters[category].insert(pattern);
        else
            filters[category].insert(pattern);
    }

    buildFilters(filters, m_filters);
    buildFilters(negfilters, m_negfilters);
}

void ResourceSortFilterProxyModel::buildFilters(const QSet<QString> parts[], QRegExp *filters)
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

#include "moc_resourcemodel.cpp"
