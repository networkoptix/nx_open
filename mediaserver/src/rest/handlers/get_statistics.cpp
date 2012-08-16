#include <QFileInfo>
#include "utils/network/tcp_connection_priv.h"
#include "rest/server/rest_server.h"
#include "core/resourcemanagment/resource_pool.h"
#include "utils/common/util.h"
#include "api/serializer/serializer.h"
#include "get_statistics.h"
#include "recorder/storage_manager.h"
#include "utils/common/performance.h"

int QnGetStatisticsHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& resultByteArray, QByteArray& contentType)
{
    Q_UNUSED(params)
    Q_UNUSED(path)
    QnStorageManager::StorageMap storages = qnStorageMan->getAllStorages();
    QString result;
    result.append("<?xml version=\"1.0\"?>\n");
    result.append("<root>\n");

    result.append("<storages>\n");

#if defined(Q_OS_WIN)
    QList<int> *hddUsage = new QList<int>();
    QnPerformance::currentHddUsage(hddUsage);
    for (int i = 0; i < hddUsage->count(); i++){
        result.append("<storage>\n");
        result.append(QString("<url>HDD%1</url>\n").arg(i));
        result.append(QString("<usage>%1</usage>\n").arg(hddUsage->at(i)));
        result.append("</storage>\n");
    }
    delete hddUsage;
#else
    // old way calculating
    foreach(QnStorageResourcePtr storage, storages)
    {
        result.append("<storage>\n");
        result.append(QString("<url>%1</url>\n").arg(storage->getUrl()));
        result.append(QString("<usage>%1</usage>\n").arg(int(storage->getAvarageWritingUsage() * 100 + 0.5)));
        result.append("</storage>\n");
    }
#endif
    result.append("</storages>\n");

    result.append("<cpuinfo>\n");
    
    result.append(QString("<model>%1</model>\n").arg(QnPerformance::cpuBrand()));
    result.append(QString("<cores>%1</cores>\n").arg(QnPerformance::cpuCoreCount()));
    result.append(QString("<usage>%1</usage>\n").arg(QnPerformance::currentCpuUsage()));
    result.append(QString("<load>%1</load>\n").arg(QnPerformance::currentCpuTotalUsage()));
    result.append("</cpuinfo>\n");

    result.append("</root>\n");

    resultByteArray = result.toUtf8();

    return CODE_OK;
}

int QnGetStatisticsHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(body)
    return executeGet(path, params, result, contentType);
}

QString QnGetStatisticsHandler::description(TCPSocket* tcpSocket) const
{
    Q_UNUSED(tcpSocket)
    QString rez;
    rez += "Returns server info: CPU usage, HDD usage e.t.c \n";
    return rez;
}
