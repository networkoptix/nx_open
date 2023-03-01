// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "storage_list_model.h"

#include <boost/algorithm/cxx11/any_of.hpp>

#include <QtGui/QPalette>

#include <client/client_globals.h>
#include <core/resource/storage_resource.h>
#include <core/resource/client_storage_resource.h>
#include <core/resource/media_server_resource.h>

#include <nx/network/socket_common.h>
#include <nx/utils/algorithm/index_of.h>

#include <nx/vms/client/desktop/style/skin.h>

namespace
{
    // TODO: #sivanov #vkutin Refactor to use HumanReadable helper class.
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

QnStorageListModel::QnStorageListModel(QObject* parent):
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

    ScopedReset reset(this);
    m_server = server;
    m_metadataStorageId = server
        ? server->metadataStorageId()
        : QnUuid();
}

QnUuid QnStorageListModel::metadataStorageId() const
{
    return m_metadataStorageId;
}

void QnStorageListModel::setMetadataStorageId(const QnUuid &id)
{
    if (m_metadataStorageId == id)
        return;

    m_metadataStorageId = id;

    // We use ActionsColumn to show which storage is used for metadata.
    emit dataChanged(
        index(0, ActionsColumn),
        index(rowCount() - 1, ActionsColumn));
}

void QnStorageListModel::updateRebuildInfo(QnServerStoragesPool pool, const nx::vms::api::StorageScanInfo& rebuildStatus)
{
    nx::vms::api::StorageScanInfo old = m_rebuildStatus[static_cast<int>(pool)];
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
    return nx::format("%1%2").args(nx::network::SocketAddress(u.host(), u.port(0)), u.path());
}

QString QnStorageListModel::displayData(const QModelIndex& index, bool forcedText) const
{
    const QnStorageModelInfo& storageData = storage(index);
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
                    case nx::vms::api::RebuildState::partial:
                        return tr("%1 (Scanning... %2%)").arg(path).arg(progress);
                    case nx::vms::api::RebuildState::full:
                        return tr("%1 (Rebuilding... %2%)").arg(path).arg(progress);
                    default:
                        break;
                }
            }

            return path;
        }

        case StoragePoolColumn:
        {
            if (!storageData.isOnline)
                return tr("Inaccessible");
            if (!storageData.isWritable)
                return tr("Reserved");
            return storageData.isBackup ? tr("Backup") : tr("Main");
        }

        case TypeColumn:
        {
            // TODO: #sivanov Make server provide a correct enumeration value instead and/or make
            // the corresponding enumeration available in the client code.
            if (storageData.storageType == "local")
                return tr("local");
            if (storageData.storageType == "ram")
                return tr("ram");
            if (storageData.storageType == "optical")
                return tr("optical");
            if (storageData.storageType == "swap")
                return tr("swap");
            if (storageData.storageType == "network")
                return tr("network");
            if (storageData.storageType == "usb")
                return tr("usb");

            // Note: Plugins can register own out-of-enum storage types (e.g. 'smb'). It should not
            // be translated in general case, though we can make a hardcode for popular plugins.
            if (storageData.storageType == "smb")
                return tr("smb");

            return storageData.storageType;
        }

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
                    // TODO: #sivanov #vkutin Refactor to use HumanReadable helper class.
                    const auto tb = storageData.totalSpace / kBytesInTb;
                    if (tb >= 1.0)
                        return tr("%1 TB").arg(QString::number(tb, 'f', 1));

                    const auto gb = storageData.totalSpace / kBytesInGB;
                    return tr("%1 GB").arg(QString::number(gb, 'f', 1));
                }
            }
        }

        case ActionsColumn:
        {
            // Calculate predefined column width.
            if (forcedText)
                return tr("Use to store analytics and motion data");

            if (storageData.id == m_metadataStorageId)
                return tr("Stores analytics and motion data");

            if (canRemoveStorage(storageData))
                return tr("Remove");

            if (couldStoreAnalytics(storageData))
                return tr("Use to store analytics and motion data");

            return QString();
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

    // Right now all active cells (i.e. cells with pointing hand cursor) have "hoverable" text.
    return showTextOnHover(index)
        ? QVariant::fromValue<int>(Qt::PointingHandCursor)
        : QVariant();
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

bool QnStorageListModel::showTextOnHover(const QModelIndex& index) const
{
    if (index.column() != ActionsColumn)
        return false;

    const QnStorageModelInfo& storageData = storage(index);
    return (storageData.id != m_metadataStorageId
        && (couldStoreAnalytics(storageData) || canRemoveStorage(storageData)));
}

QVariant QnStorageListModel::data(const QModelIndex& index, int role) const
{
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    switch (role)
    {
        case Qt::DisplayRole:
            return displayData(index, false);

        case Qt::DecorationRole:
            if (index.column() == ActionsColumn)
            {
                const auto& storage = m_storages.at(index.row());

                if (storage.id == m_metadataStorageId)
                    return qnSkin->pixmap("text_buttons/analytics.png");

                if (canRemoveStorage(storage))
                    return qnSkin->pixmap("text_buttons/trash.png");

                if (couldStoreAnalytics(storage))
                    return qnSkin->pixmap("text_buttons/analytics.png");
            }

            return QVariant();

        case Qn::ItemMouseCursorRole:
            return mouseCursorData(index);

        case Qt::CheckStateRole:
            return checkstateData(index);

        case Qn::StorageInfoDataRole:
            return QVariant::fromValue<QnStorageModelInfo>(storage(index));

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
            }
            return QVariant();

        case ShowTextOnHoverRole:
            return showTextOnHover(index);

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

        if (index.column() == ActionsColumn)
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

    auto isFullScan = [](const nx::vms::api::StorageScanInfo& status)
    {
        return status.state == nx::vms::api::RebuildState::full;
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

    if (data.id == m_metadataStorageId)
        return false;

    return data.isExternal || !data.isOnline;
}

bool QnStorageListModel::couldStoreAnalytics(const QnStorageModelInfo& data) const
{
    // TODO: #vbreus Fix QnClientStorageResource::getCapabilities() to make
    // QnStorageResource::isDbReady() method and subsequent
    // QnStorageResource::canStoreAnalytics() method work correctly in client.

    return data.isWritable && data.isOnline && data.isDbReady;
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
    return (status.state == nx::vms::api::RebuildState::full);
}

bool QnStorageListModel::isStorageInRebuild(const QnStorageModelInfo& storage) const
{
    /* Check if the current storage is in rebuild */
    for (const auto& rebuildStatus: m_rebuildStatus)
    {
        if (rebuildStatus.path == storage.url)
            return rebuildStatus.state != nx::vms::api::RebuildState::none;
    }
    return false;
}

