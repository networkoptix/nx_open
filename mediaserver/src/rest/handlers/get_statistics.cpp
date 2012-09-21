#include "get_statistics.h"

#include <QtCore/QFileInfo>

#include "utils/common/util.h"
#include "utils/monitoring/global_monitor.h"
#include "utils/network/tcp_connection_priv.h"

#include "core/resourcemanagment/resource_pool.h"

#include "api/serializer/serializer.h"

#include "rest/server/rest_server.h"
#include "recorder/storage_manager.h"

QnGetStatisticsHandler::QnGetStatisticsHandler() {
    m_monitor = new QnGlobalMonitor(QnPlatformMonitor::newInstance(), this);
}

QnGetStatisticsHandler::~QnGetStatisticsHandler() 
{

}


int QnGetStatisticsHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& resultByteArray, QByteArray& contentType)
{
    Q_UNUSED(params)
    Q_UNUSED(path)
    QnStorageManager::StorageMap storages = qnStorageMan->getAllStorages();
    QString result;
    result.append("<?xml version=\"1.0\"?>\n");
    result.append("<root>\n");

    result.append("<storages>\n");
    foreach(const QnPlatformMonitor::HddLoad &hddLoad, m_monitor->totalHddLoad()) {
        result.append("<storage>\n");
        result.append(QString("<url>%1</url>\n").arg(hddLoad.hdd.partitions));
        result.append(QString("<usage>%1</usage>\n").arg(hddLoad.load));
        result.append("</storage>\n");
    }
    result.append("</storages>\n");

    result.append("<cpuinfo>\n");
    result.append(QString("<cores>%1</cores>\n").arg(QThread::idealThreadCount()));
    result.append(QString("<load>%1</load>\n").arg(m_monitor->totalCpuUsage()));
    result.append("</cpuinfo>\n");

    result.append("<memory>\n");
    result.append(QString("<usage>%1</usage>\n").arg(m_monitor->totalRamUsage()));
    result.append("</memory>\n");

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
