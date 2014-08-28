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
}

QnTimeServerSelectionModel::QnTimeServerSelectionModel(QObject* parent /*= NULL*/):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_timeBase(0)
{
    
    auto processor = QnCommonMessageProcessor::instance();

    connect(processor, &QnCommonMessageProcessor::syncTimeChanged, this, [this](qint64 syncTime) {
        m_timeBase = syncTime;
    });

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

    connect(QnRuntimeInfoManager::instance(),   &QnRuntimeInfoManager::runtimeInfoAdded,    this, [this](const QnPeerRuntimeInfo &info) {
        if (info.data.peer.peerType != Qn::PT_Server)
            return;

        Item item;
        item.peerId = info.uuid;
        item.priority = info.data.serverTimePriority;
        item.time = -1;
        
        beginInsertRows(QModelIndex(), m_items.size(), m_items.size());
        m_items << item;
        endInsertRows();
    });
    
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

    beginResetModel();
    foreach (const QnPeerRuntimeInfo &runtimeInfo, QnRuntimeInfoManager::instance()->items()->getItems()) {
        if (runtimeInfo.data.peer.peerType != Qn::PT_Server)
            continue;
        
        Item item;
        item.peerId = runtimeInfo.uuid;
        item.priority = runtimeInfo.data.serverTimePriority;
        item.time = timeByPeer.value(runtimeInfo.uuid, -1);
        m_items << item;
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

    QnMediaServerResourcePtr server = qnResPool->getResourceById(item.peerId).dynamicCast<QnMediaServerResource>();
    if (!server)
        return QVariant();

    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();

    switch (role) {
      case Qt::DisplayRole:
      case Qt::StatusTipRole:
      case Qt::WhatsThisRole:
      case Qt::AccessibleTextRole:
      case Qt::AccessibleDescriptionRole:
      case Qt::ToolTipRole:
          if (column == Columns::NameColumn)
              return getFullResourceName(server, true);
          if (column == Columns::TimeColumn) {
              if (item.time < 0 || m_timeBase == 0)
                  return tr("Loading...");
              return QDateTime::fromMSecsSinceEpoch(currentTime - m_timeBase + item.time).toString(Qt::ISODate);
          }
          break;
      case Qt::DecorationRole:
          if (column == Columns::NameColumn)
              return qnResIconCache->icon(QnResourceIconCache::Server);
          break;
      case Qt::CheckStateRole:
          if (column == Columns::CheckboxColumn)
              return (item.priority & ec2::ApiRuntimeData::tpfPeerTimeSetByUser) > 0 ? Qt::Checked : Qt::Unchecked;
          break;
    }
    return QVariant();
}

void QnTimeServerSelectionModel::updateTime() {
    if (m_items.isEmpty())
        return;

    int column = Columns::TimeColumn;
    emit dataChanged(index(0, column), index(m_items.size() - 1, column), textRoles);
}
