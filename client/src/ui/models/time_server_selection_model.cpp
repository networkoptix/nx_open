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
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>

#include <utils/common/collection.h>
#include <utils/common/qtimespan.h>
#include <utils/common/synctime.h>
#include <utils/tz/tz.h>

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
    m_sameTimezone(false),
    m_sameTimezoneValid(false)
{
    auto processor = QnCommonMessageProcessor::instance();

    /* Handle peer time updates. */
    connect(processor, &QnCommonMessageProcessor::peerTimeChanged, this, [this](const QnUuid &peerId, qint64 syncTime, qint64 peerTime) {
        int idx = qnIndexOf(m_items, [peerId](const Item &item) {return item.peerId == peerId; });
        if (idx < 0)
            return;
        m_items[idx].offset = peerTime - syncTime;
        m_items[idx].ready = true;
        emit dataChanged(index(idx, TimeColumn), index(idx, OffsetColumn), textRoles);
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
            setSelectedServer(QnUuid());
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

        QnUuid id = resource->getId();
        int idx = qnIndexOf(m_items, [id](const Item &item){return item.peerId == id; });
        if (idx < 0)
            return;

        m_sameTimezoneValid = false;
        updateColumn(Columns::TimeColumn);
        updateColumn(Columns::OffsetColumn);

        emit dataChanged(
            this->index(idx, Columns::NameColumn), 
            this->index(idx, Columns::NameColumn),
            textRoles);
    });

    connect(qnSyncTime, &QnSyncTime::timeChanged, this, [this]{
        updateColumn(Columns::TimeColumn);
        updateColumn(Columns::OffsetColumn);
    });

    connect(context()->instance<QnWorkbenchServerTimeWatcher>(), &QnWorkbenchServerTimeWatcher::offsetsChanged, this, [this]{
        m_sameTimezoneValid = false;
        updateColumn(Columns::TimeColumn);
        updateColumn(Columns::OffsetColumn);
    });

    /* Requesting initial time. */
    QHash<QnUuid, qint64> timeByPeer;
    if (auto connection = QnAppServerConnectionFactory::getConnection2()) {
        auto timeManager = connection->getTimeManager();
        foreach(const ec2::QnPeerTimeInfo &info, timeManager->getPeerTimeInfoList())
            timeByPeer[info.peerId] = info.time;
    }

    /* Fill table with current data. */
    beginResetModel();
    foreach (const QnPeerRuntimeInfo &runtimeInfo, QnRuntimeInfoManager::instance()->items()->getItems()) {
        if (runtimeInfo.data.peer.peerType != Qn::PT_Server)
            continue;
        addItem(runtimeInfo, timeByPeer.value(runtimeInfo.uuid, 0));
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
            return tr("Server Time");
        case Columns::OffsetColumn:
            return tr("Offset");
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

    qint64 currentTime = qnSyncTime->currentMSecsSinceEpoch();

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
              if (!item.ready)
                  return tr("Synchronizing...");

              qint64 mSecsSinceEpoch = currentTime + item.offset;
              QDateTime time;
              if (sameTimezone()) {
                  time.setTimeSpec(Qt::LocalTime);
                  time.setMSecsSinceEpoch(mSecsSinceEpoch);
                  return time.toString(lit("yyyy-MM-dd HH:mm:ss"));
              } else {
                  qint64 utcOffset = server 
                      ? context()->instance<QnWorkbenchServerTimeWatcher>()->utcOffset(server)
                      : Qn::InvalidUtcOffset;

                  if (utcOffset != Qn::InvalidUtcOffset) {
                      time.setTimeSpec(Qt::OffsetFromUTC);
                      time.setUtcOffset(utcOffset / 1000);
                      time.setMSecsSinceEpoch(mSecsSinceEpoch);
                  } else {
                      time.setTimeSpec(Qt::UTC);
                      time.setMSecsSinceEpoch(mSecsSinceEpoch);
                  }
                  return time.toString(lit("yyyy-MM-dd HH:mm:ss t"));
              } 
          }
          if (column == Columns::OffsetColumn) {
              if (!item.ready)
                  return QString();
              return formattedOffset(item.offset);
          }
          break;
      case Qt::DecorationRole:
          if (column == Columns::NameColumn){
              if (QnMediaServerResource::isHiddenServer(server))
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
    item.ready = time > 0;
    if (time > 0)
        item.offset = time - qnSyncTime->currentMSecsSinceEpoch();
    m_items << item;

    m_sameTimezoneValid = false;
}

QnUuid QnTimeServerSelectionModel::selectedServer() const {
    return m_selectedServer;
}

void QnTimeServerSelectionModel::setSelectedServer(const QnUuid &serverId) {
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

QString QnTimeServerSelectionModel::formattedOffset(qint64 offsetMSec) {
    if (offsetMSec == 0)
        return lit("0.00");
    QChar sign = offsetMSec < 0 
        ? L'-'
        : L'+';

    QTimeSpan span(offsetMSec);
    span.normalize();
    return span.toApproximateString();

/*
    QString body;

    QString format = lit("%1%2%3");
    if (span > QTimeSpan::Hour)
        return format.arg(sign).arg(span.toHours(), 2, 'g', 2).arg(tr("h"));
    else if (span > QTimeSpan::Minute)
        return format.arg(sign).arg(span.toMinutes(), 2, 'g', 2).arg(tr("m"));
    else 
        return format.arg(sign).arg(span.toSecs(), 2, 'g', 2).arg(tr("s"));
*/
}

bool QnTimeServerSelectionModel::sameTimezone() const {
    if (!m_sameTimezoneValid) {
        m_sameTimezone = calculateSameTimezone();
        m_sameTimezoneValid = true;
    }
    return m_sameTimezone;  
}

bool QnTimeServerSelectionModel::calculateSameTimezone() const {
    auto watcher = context()->instance<QnWorkbenchServerTimeWatcher>();
    qint64 localOffset = nx_tz::getLocalTimeZoneOffset(); /* In minutes. */
    qint64 commonOffset = localOffset == -1 
        ? Qn::InvalidUtcOffset
        : localOffset*60;   

    foreach (const Item &item, m_items) {
        QnMediaServerResourcePtr server = qnResPool->getResourceById(item.peerId).dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        qint64 offset = watcher->utcOffset(server); /* In milliseconds. */
        if (offset == Qn::InvalidUtcOffset)
            continue;
        offset /= 1000;

        if (commonOffset == Qn::InvalidUtcOffset)
            commonOffset = offset;
        else
            if (commonOffset != offset)
                return false;
    }
    return true;
}
