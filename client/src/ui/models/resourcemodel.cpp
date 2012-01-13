#include "resourcemodel.h"

#include <QtCore/QMimeData>

#include <core/resourcemanagment/resource_pool.h>

#include "ui/skin/skin.h"

#include "ui/view_drag_and_drop.h"

#include "file_processor.h"

ResourceModel::ResourceModel(QObject *parent)
    : QStandardItemModel(parent)
{
    connect(qnResPool, SIGNAL(resourceAdded(QnResourcePtr)), this, SLOT(_q_addResource(QnResourcePtr)));
    connect(qnResPool, SIGNAL(resourceRemoved(QnResourcePtr)), this, SLOT(_q_removeResource(QnResourcePtr)));

    const QnResourceList resources = qnResPool->getResources();
    foreach (QnResourcePtr server, resources) {
        if (server->checkFlag(QnResource::server)) {
            addResource(server);
            const QnId serverId = server->getId();
            foreach (QnResourcePtr resource, resources) {
                if (resource->getParentId() == serverId && !resource->checkFlag(QnResource::server))
                    addResource(resource);
            }
        }
    }

    QHash<int, QByteArray> roles = roleNames();
    roles.insert(Qt::UserRole + 1, "id");
    roles.insert(Qt::UserRole + 2, "searchString");
    setRoleNames(roles);
}

ResourceModel::~ResourceModel()
{
}

QnResourcePtr ResourceModel::resourceFromIndex(const QModelIndex &index) const
{
    return qnResPool->getResourceById(index.data(Qt::UserRole + 1));
}

QModelIndex ResourceModel::indexFromResource(QnResourcePtr resource) const
{
    return indexFromResourceId(resource->getId().hash());
}

QModelIndex ResourceModel::indexFromResourceId(uint id) const // ### remove; use use indexFromResource() instead
{
    const QModelIndexList indexList = match(index(0, 0), Qt::UserRole + 1, id, 1, Qt::MatchExactly | Qt::MatchRecursive);
    return indexList.value(0);
}

static inline QIcon iconForResource(QnResourcePtr resource)
{
    QString iconName;
    if (resource->checkFlag(QnResource::server))
        iconName = resource->checkFlag(QnResource::remote) ? QLatin1String("server.png") : QLatin1String("home.png");
    else if (resource->checkFlag(QnResource::layout))
        iconName = QLatin1String("layout.png");
    else if (resource->checkFlag(QnResource::live_cam))
        iconName = QLatin1String("webcam.png");
    else if (resource->checkFlag(QnResource::SINGLE_SHOT))
        iconName = QLatin1String("snapshot.png");
    else if (resource->checkFlag(QnResource::ARCHIVE) || resource->checkFlag(QnResource::server_archive))
        iconName = QLatin1String("media.png");

    return !iconName.isEmpty() ? Skin::icon(iconName) : QIcon();
}

void ResourceModel::addResource(QnResourcePtr resource)
{
    if (resource->checkFlag(QnResource::server)) {
        QStandardItem *root = new QStandardItem(resource->getName());
        root->setData(resource->getId().hash(), Qt::UserRole + 1);
        root->setData(resource->toSearchString(), Qt::UserRole + 2);
        root->setIcon(iconForResource(resource));
        root->setEditable(false);
        root->setSelectable(false);
        appendRow(root);
    } else if (QStandardItem *root = itemFromResourceId(resource->getParentId().hash())) {
        QStandardItem *child = new QStandardItem(resource->getName());
        child->setData(resource->getId().hash(), Qt::UserRole + 1);
        child->setData(resource->toSearchString(), Qt::UserRole + 2);
        child->setIcon(iconForResource(resource));
        child->setEditable(false);
        root->appendRow(child);
    } else {
        qWarning("ResourceModel::addResource(): parent resource (id %d) wasn't found for resource (id %d)",
                 resource->getParentId().hash(), resource->getId().hash());
    }
}

void ResourceModel::removeResource(QnResourcePtr resource)
{
    if (QStandardItem *item = itemFromResource(resource)) {
        foreach (QStandardItem *rowItem, takeRow(item->row()))
            delete rowItem;
    }
}

void ResourceModel::_q_addResource(QnResourcePtr resource)
{
    if (sender() != qnResPool && !qnResPool->getResourceById(resource->getId())) {
        qnResPool->addResource(resource); // emits resourceAdded; thus return
        return;
    }

    if (itemFromResource(resource))
        return; // avoid duplicates

    addResource(resource);
}

void ResourceModel::_q_removeResource(QnResourcePtr resource)
{
    if (sender() != qnResPool && qnResPool->getResourceById(resource->getId())) {
        qnResPool->removeResource(resource); // emits resourceRemoved; thus return
        return;
    }

    removeResource(resource);
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
                QnResourcePtr resource = resourceFromIndex(index);
                if (resource)
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
        if (resource->checkFlag(QnResource::local) && !resource->checkFlag(QnResource::server))
            addResource(resource);
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
    return qnResPool->getResourceById(index.data(Qt::UserRole + 1));
}

QModelIndex ResourceSortFilterProxyModel::indexFromResource(QnResourcePtr resource) const
{
    const QModelIndexList indexList = match(index(0, 0), Qt::UserRole + 1, resource->getId().hash(), 1, Qt::MatchExactly | Qt::MatchRecursive);
    return indexList.value(0);
}

static inline bool matchesCategoryFilter(const QList<QRegExp> &filters, const QString &value)
{
    for (int i = 0; i < filters.size(); ++i) {
        if (value.contains(filters[i]))
            return true;
    }

    return false;
}

bool ResourceSortFilterProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    if (!source_parent.isValid())
        return true;

    if (m_parsedFilterString != filterRegExp().pattern())
        const_cast<ResourceSortFilterProxyModel *>(this)->parseFilterString();
    if (m_parsedFilterString.isEmpty())
        return false;

    QnResourcePtr resource = resourceFromIndex(sourceModel()->index(source_row, 0, source_parent));
    if (!resource)
        return false;

    if (m_flagsFilter != 0) {
        if (resource->flags() & m_flagsFilter)
            return true;
    }

    if (!m_filters[Id].isEmpty()) {
        const QString id = QString::number(resource->getId().hash());
        if (matchesCategoryFilter(m_filters[Id], id))
            return true;
    }

    if (!m_filters[Name].isEmpty()) {
        const QString name = resource->getName();
        if (matchesCategoryFilter(m_filters[Name], name))
            return true;
    }

    if (!m_filters[Tags].isEmpty()) {
        QString tags = resource->tagList().join(QLatin1String("\",\""));
        if (!tags.isEmpty())
            tags = QLatin1Char('"') + tags + QLatin1Char('"');
        if (matchesCategoryFilter(m_filters[Tags], tags))
            return true;
    }

    if (!m_filters[Text].isEmpty()) {
        const int filter_role = filterRole();
        const int filter_column = filterKeyColumn();
        if (filter_column == -1) {
            const int columnCount = sourceModel()->columnCount(source_parent);
            for (int column = 0; column < columnCount; ++column) {
                const QModelIndex source_index = sourceModel()->index(source_row, column, source_parent);
                const QString text = source_index.data(filter_role).toString();
                if (matchesCategoryFilter(m_filters[Text], text))
                    return true;
            }
        } else {
            const QModelIndex source_index = sourceModel()->index(source_row, filter_column, source_parent);
            if (source_index.isValid()) { // the column may not exist
                const QString text = source_index.data(filter_role).toString();
                if (matchesCategoryFilter(m_filters[Text], text))
                    return true;
            }
        }
    }

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
    for (uint i = 0; i < NumFilterCategories; ++i)
        m_filters[i].clear();

    m_parsedFilterString = filterRegExp().pattern();
    if (m_parsedFilterString.isEmpty())
        return;

    const QRegExp::PatternSyntax patternSyntax = filterRegExp().patternSyntax();
    if (patternSyntax != QRegExp::FixedString && patternSyntax != QRegExp::Wildcard && patternSyntax != QRegExp::WildcardUnix)
        return;

    QString filterString = normalizedFilterString(m_parsedFilterString);
    foreach (const QString &filter, filterString.split(QRegExp(QLatin1String("[\\s\\|;+]")), QString::SkipEmptyParts)) {
        const int idx = filter.indexOf(QLatin1Char(':'));
        FilterCategory category = Text;
        const QString pattern = filter.mid(idx + 1);
        if (idx != -1) {
            const QString key = filter.left(idx).toLower();
            if (key == QLatin1String("id"))
                category = Id;
            else if (key == QLatin1String("name"))
                category = Name;
            else if (key == QLatin1String("tag"))
                category = Tags;
        } else {
            if (pattern == QLatin1String("live"))
                m_flagsFilter |= QnResource::live;
        }
        const QRegExp rx(pattern, category == Id ? Qt::CaseSensitive : Qt::CaseInsensitive, patternSyntax);
        m_filters[category].append(rx);
    }
}
