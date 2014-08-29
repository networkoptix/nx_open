#include "time_server_selection_model.h"

#include <api/app_server_connection.h>
#include <api/common_message_processor.h>
#include <api/runtime_info_manager.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_name.h>

#include <nx_ec/ec_api.h>
#include <nx_ec/data/api_runtime_data.h>

#include <ui/style/resource_icon_cache.h>

#include <utils/common/collection.h>

namespace {
    QVector<int> textRoles = QVector<int>() 
        << Qt::DisplayRole 
        << Qt::StatusTipRole 
        << Qt::WhatsThisRole
        << Qt::AccessibleTextRole
        << Qt::AccessibleDescriptionRole
        << Qt::ToolTipRole;

    QVector<int> checkboxRoles = QVector<int>()
        << Qt::DisplayRole 
        << Qt::CheckStateRole;
}

QnTimeServerSelectionModel::QnTimeServerSelectionModel(QObject* parent /*= NULL*/):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_timeBase(0)
{
    auto processor = QnCommonMessageProcessor::instance();

    /* Handle synchronized time updates. */
    connect(processor, &QnCommonMessageProcessor::syncTimeChanged, this, [this](qint64 syncTime) {
        m_timeBase = syncTime;
    });

    /* Handle peer time updates. */
    connect(processor, &QnCommonMessageProcessor::peerTimeChanged, this, [this](const QUuid &peerId, qint64 syncTime, qint64 peerTime) {
        bool syncTimeChanged = m_timeBase != syncTime;
        m_timeBase = syncTime;
        for (int i = 0; i < m_items.size(); ++i) {
            if (m_items[i].peerId != peerId)
                continue;;
            m_items[i].time = peerTime;
            if (!syncTimeChanged) {
                emit dataChanged(index(i, TimeColumn), index(i, TimeColumn), textRoles);
                return; //update only what needed
            }
        }

        if (syncTimeChanged)
            updateTime();
    });

    /* Handle adding new online server peers. */
    connect(QnRuntimeInfoManager::instance(),   &QnRuntimeInfoManager::runtimeInfoAdded,    this, [this](const QnPeerRuntimeInfo &info) {
        if (info.data.peer.peerType != Qn::PT_Server)
            return;

        beginInsertRows(QModelIndex(), m_items.size(), m_items.size());
        addItem(info);
        endInsertRows();
    });

    /* Handle changing server peers priority (selection). */
    connect(QnRuntimeInfoManager::instance(),   &QnRuntimeInfoManager::runtimeInfoChanged,  this, [this](const QnPeerRuntimeInfo &info) {
        if (info.data.peer.peerType != Qn::PT_Server)
            return;

        if (isSelected(info.data.serverTimePriority))
            setSelectedServer(info.uuid);
        else if (info.uuid == m_selectedServer)
            setSelectedServer(QUuid());
    });
    
    /* Handle removing online server peers. */
    connect(QnRuntimeInfoManager::instance(),   &QnRuntimeInfoManager::runtimeInfoRemoved,  this,  [this](const QnPeerRuntimeInfo &info) {
        if (info.data.peer.peerType != Qn::PT_Server)
            return;

        int idx = qnIndexOf(m_items, [info](const Item &item){return item.peerId == info.uuid; });
        if (idx < 0)
            return;

        beginRemoveRows(QModelIndex(), idx, idx);
        m_items.removeAt(idx);
        endRemoveRows();
    });

    /* Handle adding new servers (to display name correctly). */
    connect(qnResPool, &QnResourcePool::resourceAdded, this, [this](const QnResourcePtr &resource){
        if (!resource.dynamicCast<QnMediaServerResource>())
            return;

        QUuid id = resource->getId();
        int idx = qnIndexOf(m_items, [id](const Item &item){return item.peerId == id; });
        if (idx < 0)
            return;

        emit dataChanged(
            this->index(idx, Columns::NameColumn), 
            this->index(idx, Columns::NameColumn),
            textRoles);
    });


    /* Requesting initial time. */
    QHash<QUuid, qint64> timeByPeer;
    if (auto connection = QnAppServerConnectionFactory::getConnection2()) {
        auto timeManager = connection->getTimeManager();
        timeManager->getCurrentTime(this, [this](int handle, ec2::ErrorCode errCode, qint64 syncTime) {
            Q_UNUSED(handle);
            if (errCode != ec2::ErrorCode::ok)
                return;
            m_timeBase = syncTime;
        });

        foreach(const ec2::QnPeerTimeInfo &info, timeManager->getPeerTimeInfoList())
            timeByPeer[info.peerId] = info.time;
    }

    /* Fill table with current data. */
    beginResetModel();
    foreach (const QnPeerRuntimeInfo &runtimeInfo, QnRuntimeInfoManager::instance()->items()->getItems()) {
        if (runtimeInfo.data.peer.peerType != Qn::PT_Server)
            continue;
        addItem(runtimeInfo, timeByPeer.value(runtimeInfo.uuid, -1));
    }
    endResetModel();
}


int QnTimeServerSelectionModel::columnCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;

    return Columns::ColumnCount;
}

int QnTimeServerSelectionModel::rowCount(const QModelIndex &parent) const {
    if(parent.isValid())
        return 0;

    return m_items.size();
}

QVariant QnTimeServerSelectionModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case Columns::CheckboxColumn:
            return QString();
        case Columns::NameColumn:
            return tr("Server");
        case Columns::TimeColumn:
            return tr("Local Time");
        default:
            break;
        }
    }
    return QVariant();
}

QVariant QnTimeServerSelectionModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    if (index.row() < 0 || index.row() >= m_items.size())
        return QVariant();

    Columns column = static_cast<Columns>(index.column());
    Item item = m_items[index.row()];

    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();

    QnMediaServerResourcePtr server = qnResPool->getResourceById(item.peerId).dynamicCast<QnMediaServerResource>();
    QString title = server ? getFullResourceName(server, true) : tr("Server");

    switch (role) {
      case Qt::DisplayRole:
      case Qt::StatusTipRole:
      case Qt::WhatsThisRole:
      case Qt::AccessibleTextRole:
      case Qt::AccessibleDescriptionRole:
      case Qt::ToolTipRole:
          if (column == Columns::NameColumn)
              return title;
          if (column == Columns::TimeColumn) {
              if (item.time < 0 || m_timeBase == 0)
                  return tr("Synchronizing...");
              return QDateTime::fromMSecsSinceEpoch(currentTime - m_timeBase + item.time).toString(Qt::ISODate);
          }
          break;
      case Qt::DecorationRole:
          if (column == Columns::NameColumn){
              if (QnMediaServerResource::isEdgeServer(server))
                  return qnResIconCache->icon(QnResourceIconCache::Camera);
              return qnResIconCache->icon(QnResourceIconCache::Server);
          }
          break;
      case Qt::CheckStateRole:
          if (column == Columns::CheckboxColumn)
              return item.peerId == m_selectedServer ? Qt::Checked : Qt::Unchecked;
          break;
      case Qn::PriorityRole:
          return item.priority;
    }
    return QVariant();
}

bool QnTimeServerSelectionModel::setData(const QModelIndex &index, const QVariant &value, int role /*= Qt::EditRole*/) {
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return 0;

    if (index.row() < 0 || index.row() >= m_items.size())
        return 0;

    if (index.column() != Columns::CheckboxColumn || role != Qt::CheckStateRole)
        return false;

    Qt::CheckState state = static_cast<Qt::CheckState>(value.toInt());

    // do not allow to uncheck element
    if (state == Qt::Unchecked)
        return false;

    setSelectedServer(m_items[index.row()].peerId);
    return true;
}

Qt::ItemFlags QnTimeServerSelectionModel::flags(const QModelIndex &index) const  {
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return 0;

    if (index.row() < 0 || index.row() >= m_items.size())
        return 0;

    Columns column = static_cast<Columns>(index.column());
    Item item = m_items[index.row()];

    if (column == Columns::CheckboxColumn)
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEditable;

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void QnTimeServerSelectionModel::updateTime() {
    updateColumn(Columns::TimeColumn);
}

void QnTimeServerSelectionModel::addItem(const QnPeerRuntimeInfo &info, qint64 time) {
    Item item;
    item.peerId = info.uuid;
    item.priority = info.data.serverTimePriority;
    if (isSelected(item.priority))
        m_selectedServer = item.peerId;
    item.time = time;
    m_items << item;
}

QUuid QnTimeServerSelectionModel::selectedServer() const {
    return m_selectedServer;
}

void QnTimeServerSelectionModel::setSelectedServer(const QUuid &serverId) {
    if (m_selectedServer == serverId)
        return;

    if (!serverId.isNull()) {
        int idx = qnIndexOf(m_items, [serverId](const Item &item){return item.peerId == serverId; });
        if (idx < 0)
            return;
    }

    m_selectedServer = serverId;
    updateColumn(Columns::CheckboxColumn);
}

void QnTimeServerSelectionModel::updateColumn(Columns column) {
    if (m_items.isEmpty())
        return;

    QVector<int> roles = column == Columns::CheckboxColumn 
        ? checkboxRoles
        : textRoles;

    emit dataChanged(index(0, column), index(m_items.size() - 1, column), roles);
}

bool QnTimeServerSelectionModel::isSelected(quint64 priority) {
    return (priority & ec2::ApiRuntimeData::tpfPeerTimeSetByUser) > 0;
}
