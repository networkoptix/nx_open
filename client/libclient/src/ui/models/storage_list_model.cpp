#include "storage_list_model.h"

#include <core/resource/storage_resource.h>
#include <core/resource/client_storage_resource.h>
#include <core/resource/media_server_resource.h>

#include <nx/utils/collection.h>

namespace
{
    const qreal kBytesInGB = 1024.0 * 1024.0 * 1024.0;

    int storageIndex(const QnStorageModelInfoList& list, const QnStorageModelInfo& storage)
    {
        auto byId = [storage](const QnStorageModelInfo& info)  { return storage.id == info.id;  };
        return qnIndexOf(list, byId);
    }

    int storageIndex(const QnStorageModelInfoList& list, const QString& path)
    {
        auto byPath = [path](const QnStorageModelInfo& info)  { return path == info.url;  };
        return qnIndexOf(list, byPath);
    }

} // anonymous namespace

// ------------------ QnStorageListMode --------------------------

QnStorageListModel::QnStorageListModel(QObject* parent) :
    base_type(parent),
    m_server(),
    m_storages(),
    m_rebuildStatus(),
    m_readOnly(false),
    m_linkBrush(QPalette().link()),
    m_linkFont()
{
    m_linkFont.setUnderline(true);
}

QnStorageListModel::~QnStorageListModel()
{
}

void QnStorageListModel::setStorages(const QnStorageModelInfoList& storages)
{
    ScopedReset reset(this);
    m_storages = storages;
    sortStorages();
}

void QnStorageListModel::addStorage(const QnStorageModelInfo& storage)
{
    ScopedReset reset(this);
    int idx = storageIndex(m_storages, storage);
    if (idx >= 0)   /* Storage already exists, updating fields. */
        m_storages[idx] = storage;
    else
        m_storages.push_back(storage);
    sortStorages();
}

void QnStorageListModel::updateStorage(const QnStorageModelInfo& storage)
{
    int idx = storageIndex(m_storages, storage);
    if (idx < 0)
        return;

    ScopedReset reset(this);
    m_storages[idx] = storage;
}


void QnStorageListModel::removeStorage(const QnStorageModelInfo& storage)
{
    int idx = storageIndex(m_storages, storage);
    if (idx < 0)
        return;

    ScopedReset reset(this);
    m_storages.removeAt(idx);
}

QnMediaServerResourcePtr QnStorageListModel::server() const
{
    return m_server;
}

void QnStorageListModel::setServer(const QnMediaServerResourcePtr& server)
{
    if (m_server == server)
        return;

    ScopedReset reset(this);
    m_server = server;
}

void QnStorageListModel::updateRebuildInfo(QnServerStoragesPool pool, const QnStorageScanData& rebuildStatus)
{
    QnStorageScanData old = m_rebuildStatus[static_cast<int>(pool)];
    int oldRow = storageIndex(m_storages, old.path);

    m_rebuildStatus[static_cast<int>(pool)] = rebuildStatus;
    int newRow = storageIndex(m_storages, rebuildStatus.path);

    if (oldRow >= 0)
        emit dataChanged(index(oldRow, 0), index(oldRow, ColumnCount - 1));

    if (newRow >= 0 && newRow != oldRow)
        emit dataChanged(index(newRow, 0), index(newRow, ColumnCount - 1));
}


void QnStorageListModel::sortStorages()
{
    std::sort(m_storages.begin(), m_storages.end(),
        [](const QnStorageModelInfo& left, const QnStorageModelInfo& right)
        {
            /* Local storages should go first. */
            if (left.isExternal != right.isExternal)
                return right.isExternal;

            /* Group storages by plugin. */
            if (left.storageType != right.storageType)
                return QString::compare(left.storageType, right.storageType, Qt::CaseInsensitive) < 0;

            return QString::compare(left.url, right.url, Qt::CaseInsensitive) < 0;
        });
}

QnStorageModelInfo QnStorageListModel::storage(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() >= m_storages.size())
        return QnStorageModelInfo();

    return m_storages[index.row()];
}

QnStorageModelInfoList QnStorageListModel::storages() const
{
    return m_storages;
}

int QnStorageListModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
        return m_storages.size();

    return 0;
}

int QnStorageListModel::columnCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
        return ColumnCount;

    return 0;
}

QString urlPath(const QString& url)
{
    if (url.indexOf(lit("://")) > 0)
    {
        QUrl u(url);
        return u.host() + (u.port() != -1 ? lit(":").arg(u.port()) : lit(""))  + u.path();
    }

    return url;
}

QString QnStorageListModel::displayData(const QModelIndex& index, bool forcedText) const
{
    QnStorageModelInfo storageData = storage(index);
    switch (index.column())
    {
        case CheckBoxColumn:
            return QString();

        case UrlColumn:
        {
            QString path = urlPath(storageData.url);
            if (!storageData.isOnline && storageData.isWritable && storageIsActive(storageData))
                return tr("%1 (Checking...)").arg(path);

            for (const auto& rebuildStatus: m_rebuildStatus)
            {
                if (rebuildStatus.path != storageData.url)
                    continue;

                int progress = static_cast<int>(rebuildStatus.progress * 100 + 0.5);
                switch (rebuildStatus.state)
                {
                case Qn::RebuildState_PartialScan:
                    return tr("%1 (Scanning... %2%)").arg(path).arg(progress);
                case Qn::RebuildState_FullScan:
                    return tr("%1 (Rebuilding... %2%)").arg(path).arg(progress);
                default:
                    break;
                }
            }

            return path;
        }

        case TypeColumn:
            return storageData.storageType;

        case TotalSpaceColumn:
        {
            switch (storageData.totalSpace)
            {
                case QnStorageResource::kUnknownSize:
                    return tr("Invalid storage");
                case QnStorageResource::kSizeDetectionOmitted:
                    return tr("Loading...");
                default:
                    return tr("%1 Gb").arg(QString::number(storageData.totalSpace / kBytesInGB, 'f', 1));
            }
        }

        case RemoveActionColumn:
        {
            /* Calculate predefined column width */
            if (forcedText)
                return tr("Remove");

            return canRemoveStorage(storageData)
                ? tr("Remove")
                : QString();
        }

        case ChangeGroupActionColumn:
        {
            /* Calculate predefined column width */
            if (forcedText)
                return tr("Use as backup storage");

            if (m_readOnly)
                return QString();

            if (!storageData.isWritable)
                return tr("Inaccessible");

            if (!canMoveStorage(storageData))
                return QString();

            return storageData.isBackup
                ? tr("Use as main storage")
                : canMoveStorage(storageData)
                ? tr("Use as backup storage")
                : QString();
        }

        default:
            break;
    }

    return QString();
}

QVariant QnStorageListModel::fontData(const QModelIndex& index) const
{
    if (m_readOnly)
        return QVariant();

    QnStorageModelInfo storageData = storage(index);

    if (index.column() == RemoveActionColumn && canRemoveStorage(storageData))
        return m_linkFont;

    if (index.column() == ChangeGroupActionColumn && canMoveStorage(storageData))
        return m_linkFont;

    return QVariant();
}

QVariant QnStorageListModel::foregroundData(const QModelIndex& index) const
{
    if (m_readOnly)
        return QVariant();

    QnStorageModelInfo storageData = storage(index);

    if (index.column() == RemoveActionColumn && canRemoveStorage(storageData))
        return m_linkBrush;

    if (index.column() == ChangeGroupActionColumn && canMoveStorage(storageData))
        return m_linkBrush;

    return QVariant();
}

QVariant QnStorageListModel::mouseCursorData(const QModelIndex& index) const
{
    if (m_readOnly)
        return QVariant();

    if (index.column() == RemoveActionColumn || index.column() == ChangeGroupActionColumn)
        if (!index.data(Qt::DisplayRole).toString().isEmpty())
            return QVariant::fromValue<int>(Qt::PointingHandCursor);

    return QVariant();
}

QVariant QnStorageListModel::checkstateData(const QModelIndex& index) const
{
    if (index.column() == CheckBoxColumn)
    {
        QnStorageModelInfo storageData = storage(index);
        return storageData.isUsed && storageData.isWritable
            ? Qt::Checked
            : Qt::Unchecked;
    }
    return QVariant();
}

QVariant QnStorageListModel::data(const QModelIndex& index, int role) const
{
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    switch(role)
    {
        case Qt::DisplayRole:
            return displayData(index, false);
        case Qn::TextWidthDataRole:
            return displayData(index, true);
        case Qt::FontRole:
            return fontData(index);
        case Qt::ForegroundRole:
            return foregroundData(index);
        case Qn::ItemMouseCursorRole:
            return mouseCursorData(index);
        case Qt::CheckStateRole:
            return checkstateData(index);
        case Qn::StorageInfoDataRole:
            return QVariant::fromValue<QnStorageModelInfo>(storage(index));
        default:
            return QVariant();;
    }
}

bool QnStorageListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()) || m_readOnly)
        return false;

    QnStorageModelInfo& storageData = m_storages[index.row()];
    if (!storageData.isWritable)
        return false;

    if (role == Qt::CheckStateRole)
    {
        storageData.isUsed = (value == Qt::Checked);
        emit dataChanged(index, index, QVector<int>() << role);
        return true;
    }

    return base_type::setData(index, value, role);
}

Qt::ItemFlags QnStorageListModel::flags(const QModelIndex& index) const
{

    auto isEnabled = [this](const QModelIndex& index)
    {
        if (m_readOnly)
            return false;

        if (index.column() == RemoveActionColumn)
            return true;

        QnStorageModelInfo storageData = storage(index);
        if (!storageData.isWritable)
            return false;

        if (isStorageInRebuild(storageData))
            return false;

        if (isStoragePoolInRebuild(storageData))
            return false;

        return (storageData.isOnline
            || !storageIsActive(storageData)
            ||  index.column() == ChangeGroupActionColumn);
    };

    Qt::ItemFlags flags = Qt::ItemIsSelectable;

    if (isEnabled(index))
        flags |= Qt::ItemIsEnabled;

    if (index.column() == CheckBoxColumn)
        flags |= Qt::ItemIsUserCheckable;

    return flags;
}

bool QnStorageListModel::isReadOnly() const
{
    return m_readOnly;
}

void QnStorageListModel::setReadOnly(bool readOnly)
{
    if (m_readOnly == readOnly)
        return;

    ScopedReset reset(this);
    m_readOnly = readOnly;
}

bool QnStorageListModel::canMoveStorage(const QnStorageModelInfo& data) const
{
    using boost::algorithm::any_of;

    if (m_readOnly)
        return false;

    if (isStorageInRebuild(data))
        return false;

    auto isFullScan = [](const QnStorageScanData& status)
    {
        return status.state == Qn::RebuildState_FullScan;
    };

    if (any_of(m_rebuildStatus, isFullScan))
        return false;

    if (!data.isWritable)
        return false;

    if (!storageIsActive(data))
        return true;

    if (!data.isOnline)
        return false;

    if (data.isBackup)
        return true;

    auto isWritable = [data](const QnStorageModelInfo& other)
    {
        /* Do not count subject to remove. */
        if (other.url == data.url)
            return false;

        /* Do not count storages from another pool. */
        if (other.isBackup != data.isBackup)
            return false;

        if (!other.isWritable)
            return false;

        return true;
    };

    /* Check that at least one writable storage left in the main pool. */
    return any_of(m_storages, isWritable);
}

bool QnStorageListModel::canRemoveStorage(const QnStorageModelInfo& data) const
{
    if (m_readOnly)
        return false;

    if (isStorageInRebuild(data))
        return false;

    if (isStoragePoolInRebuild(data))
        return false;

    return data.isExternal || !data.isWritable;
}

bool QnStorageListModel::storageIsActive(const QnStorageModelInfo& data) const
{
    if (!m_server)
        return false;

    QnClientStorageResourcePtr storage = m_server->getStorageByUrl(data.url).dynamicCast<QnClientStorageResource>();
    if (!storage)
        return false;

    return storage->isActive();
}

bool QnStorageListModel::isStoragePoolInRebuild(const QnStorageModelInfo& storage) const
{
    QnServerStoragesPool pool = storage.isBackup
        ? QnServerStoragesPool::Backup
        : QnServerStoragesPool::Main;

    auto status = m_rebuildStatus[static_cast<int>(pool)];

    /* Check if the whole section is in rebuild. */
    return (status.state == Qn::RebuildState_FullScan);
}


bool QnStorageListModel::isStorageInRebuild(const QnStorageModelInfo& storage) const
{
    /* Check if the current storage is in rebuild */
    for (const auto& rebuildStatus: m_rebuildStatus)
    {
        if (rebuildStatus.path == storage.url)
            return rebuildStatus.state != Qn::RebuildState_None;
    }
    return false;
}

