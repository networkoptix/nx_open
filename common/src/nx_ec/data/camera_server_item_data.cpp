#include "camera_server_item_data.h"

namespace ec2
{

void ApiCameraServerItemData::fromResource(const QnCameraHistoryItem& item)
{
    physicalId = item.physicalId;
    serverGuid = item.mediaServerGuid;
    timestamp = item.timestamp;
}

void ApiCameraServerItemDataList::loadFromQuery(QSqlQuery& query) 
{ 
    QN_QUERY_TO_DATA_OBJECT(query, ApiCameraServerItemData, data, ApiCameraServerItemFields) 
}

void ApiCameraServerItemDataList::toResourceList(QnCameraHistoryList& cameraServerItems) const
{
    typedef QMap<qint64, QString> TimestampGuid;
    typedef QMap<QString, TimestampGuid> HistoryType;

    // CameraMAC -> (Timestamp -> ServerGuid)
    HistoryType history;

    // Fill temporary history map
    for (std::vector<ApiCameraServerItemData>::const_iterator ci = data.begin(); ci != data.end(); ++ci)
    {
        const ApiCameraServerItemData& pb_item = *ci;
        history[pb_item.physicalId][pb_item.timestamp] = pb_item.serverGuid;
    }

    for(HistoryType::const_iterator ci = history.begin(); ci != history.end(); ++ci)
    {
        QnCameraHistoryPtr cameraHistory = QnCameraHistoryPtr(new QnCameraHistory());

        if (ci.value().isEmpty())
            continue;

        QMapIterator<qint64, QString> camit(ci.value());
        camit.toFront();

        qint64 duration;
        cameraHistory->setPhysicalId(ci.key());
        while (camit.hasNext())
        {
            camit.next();

            if (camit.hasNext())
                duration = camit.peekNext().key() - camit.key();
            else
                duration = -1;

            cameraHistory->addTimePeriod(QnCameraTimePeriod(camit.key(), duration, camit.value()));
        }

        cameraServerItems.append(cameraHistory);
    }
}

}
