#include "storage_list_model.h"

#include <core/resource/storage_resource.h>
#include <core/resource/client_storage_resource.h>
#include <core/resource/media_server_resource.h>

#include <nx/network/socket_common.h>
#include <nx/utils/algorithm/index_of.h>

namespace
{
    // TODO: #GDM #3.1 move out strings and logic to separate class (string.h:bytesToString)
    const qreal kBytesInGB = 1024.0 * 1024.0 * 1024.0;
    const qreal kBytesInTb = 1024.0 * kBytesInGB;

    int storageIndex(const QnStorageModelInfoList& list, const QnStorageModelInfo& storage)
    {
        auto byId = [storage](const QnStorageModelInfo& info)  { return storage.id == info.id;  };
        return nx::utils::algorithm::index_of(list, byId);
    }

    int storageIndex(const QnStorageModelInfoList& list, const QString& path)
    {
        auto byPath = [path](const QnStorageModelInfo& info)  { return path == info.url;  };
        return nx::utils::algorithm::index_of(list, byPath);
    }

} // anonymous namespace

// ------------------ QnStorageListMode --------------------------

QnStorageListModel::QnStorageListModel(QObject* parent) :
    base_type(parent),
    m_server(),
    m_storages(),
    m_rebuildStatus(),
    m_readOnly(false),
    m_linkBrush(QPalette().link())
{
}

QnStorageListModel::~QnStorageListModel()
{
}

void QnStorageListModel::setStorages(const QnStorageModelInfoList& storages)
{
    ScopedReset reset(this);
    m_storages = storages;
    m_checkedStorages.clear();
}

void QnStorageListModel::addStorage(const QnStorageModelInfo& storage)
{
    if (updateStorage(storage))
        return;

    ScopedInsertRows insertRows(this, QModelIndex(), m_storages.size(), m_storages.size());
    m_storages.push_back(storage);
}

bool QnStorageListModel::updateStorage(const QnStorageModelInfo& storage)
{
    int row = storageIndex(m_storages, storage);
    if (row < 0)
        return false;

    m_storages[row] = storage;
    emit dataChanged(index(row, 0), index(row, ColumnCount - 1));

    return true;
}

void QnStorageListModel::removeStorage(const QnStorageModelInfo& storage)
{
    int row = storageIndex(m_storages, storage);
    if (row < 0)
        return;

    ScopedRemoveRows removeRows(this, QModelIndex(), row, row);
    m_checkedStorages.remove(m_storages[row].id);
    m_storages.removeAt(row);
}

QnMediaServerResourcePtr QnStorageListModel::server() const
{
    return m_server;
}

void QnStorageListModel::setServer(const QnMediaServerResourcePtr& server)
{
    if (m_server == server)
        return;

    ScopedReset reset(this); // TODO: #common Do we need this reset?
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
    return parent.isValid() ? 0 : m_storages.size();
}

int QnStorageListModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QString urlPath(const QString& url)
{
    if (url.indexOf(lit("://")) <= 0)
        return url;

    const QUrl u(url);
    return SocketAddress(u.host(), u.port(0)).toString() + u.path();
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

        case StoragePoolColumn:
            if (!storageData.isOnline)
                return tr("Inaccessible");
            if (!storageData.isWritable)
                return lit("Reserved");
            return storageData.isBackup ? tr("Backup") : tr("Main");

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
                {
                    // TODO: #GDM #3.1 move out strings and logic to separate class (string.h:bytesToString)
                    const auto tb = storageData.totalSpace / kBytesInTb;
                    if (tb >= 1.0)
                        return lit("%1 TB").arg(QString::number(tb, 'f', 1));

                    const auto gb = storageData.totalSpace / kBytesInGB;
                    return tr("%1 GB").arg(QString::number(gb, 'f', 1));
                }
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

        default:
            break;
    }

    return QString();
}

QVariant QnStorageListModel::mouseCursorData(const QModelIndex& index) const
{
    if (m_readOnly)
        return QVariant();

    if (index.column() == RemoveActionColumn)
        if (!index.data(Qt::DisplayRole).toString().isEmpty())
            return QVariant::fromValue<int>(Qt::PointingHandCursor);

    return QVariant();
}

QVariant QnStorageListModel::checkstateData(const QModelIndex& index) const
{
    if (index.column() != CheckBoxColumn)
        return QVariant();

    QnStorageModelInfo storageData = storage(index);
    return storageData.isUsed && storageData.isWritable
        ? Qt::Checked
        : Qt::Unchecked;
}

QVariant QnStorageListModel::data(const QModelIndex& index, int role) const
{
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    switch (role)
    {
        case Qt::DisplayRole:
            return displayData(index, false);

        case Qn::ItemMouseCursorRole:
            return mouseCursorData(index);

        case Qt::CheckStateRole:
            return checkstateData(index);

        case Qn::StorageInfoDataRole:
            return QVariant::fromValue<QnStorageModelInfo>(storage(index));

        case Qt::TextAlignmentRole:
            if (index.column() == TotalSpaceColumn)
                return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
            return QVariant();

        case Qt::ToolTipRole:
            if (index.column() == StoragePoolColumn)
            {
                const auto storageData = storage(index);
                if (storageData.isOnline && !storageData.isWritable)
                {
                    const auto text = tr("Too small and system partitions are reserved and not"
                        " used for writing if there is enough other storage space available.");

                    /* HTML tooltips are word wrapped: */
                    return lit("<span>%1</span").arg(text.toHtmlEscaped());
                }

                return QVariant();
            }

        default:
            return QVariant();
    }
}

bool QnStorageListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()) || m_readOnly)
        return false;

    QnStorageModelInfo& storageData = m_storages[index.row()];
    if (!storageData.isWritable)
        return false;

    if (role == Qt::CheckStateRole && index.column() == CheckBoxColumn)
    {
        bool checked = value == Qt::Checked;
        if (storageData.isUsed == checked)
            return false;

        storageData.isUsed = checked;

        emit dataChanged(index, index, { Qt::CheckStateRole });
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

        if (isStorageInRebuild(storageData))
            return false;

        if (isStoragePoolInRebuild(storageData))
            return false;

        if (index.column() == CheckBoxColumn && !storageData.isWritable)
            return false;

        return storageData.isOnline ||
            !storageIsActive(storageData);
    };

    Qt::ItemFlags flags = Qt::ItemIsSelectable;

    if (isEnabled(index))
        flags |= Qt::ItemIsEnabled;

    if (index.column() == StoragePoolColumn)
    {
        auto s = storage(index);
        if (s.isWritable && canChangeStoragePool(s))
            flags |= Qt::ItemIsEditable;
    }

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

bool QnStorageListModel::canChangeStoragePool(const QnStorageModelInfo& data) const
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

    return data.isExternal || !data.isOnline;
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

