#include "statistics_rest_handler.h"

#include "utils/common/util.h"
#include "utils/network/tcp_connection_priv.h"
#include "platform/platform_abstraction.h"


QnStatisticsRestHandler::QnStatisticsRestHandler() {
    m_monitor = qnPlatform->monitor();
}

QnStatisticsRestHandler::~QnStatisticsRestHandler() {
    return;
}

int QnStatisticsRestHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& resultByteArray, QByteArray& contentType)
{
    Q_UNUSED(params)
    Q_UNUSED(path)
    Q_UNUSED(contentType)

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

    result.append("<network>\n");
    foreach(const QnPlatformMonitor::NetworkLoad &networkLoad, m_monitor->totalNetworkLoad()) {
        result.append("<interface>\n");
        result.append(QString("<name>%1</name>\n").arg(networkLoad.interfaceName));
        result.append(QString("<type>%1</type>\n").arg(static_cast<int>(networkLoad.type)));
        result.append(QString("<in>%1</in>\n").arg(networkLoad.bytesPerSecIn));
        result.append(QString("<out>%1</out>\n").arg(networkLoad.bytesPerSecOut));
        result.append(QString("<max>%1</max>\n").arg(networkLoad.bytesPerSecMax));
        result.append("</interface>\n");
    }
    result.append("</network>\n");

    result.append("<misc>\n");
    result.append(QString("<uptimeMs>%1</uptimeMs>\n").arg(m_monitor->upTimeMs()));
    result.append("</misc>\n");

    result.append("<params>\n");
    result.append(QString("<updatePeriod>%1</updatePeriod>\n").arg(m_monitor->updatePeriod()));
    result.append("</params>\n");

    result.append("</root>\n");

    resultByteArray = result.toUtf8();

    return CODE_OK;
}

int QnStatisticsRestHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& /*body*/, const QByteArray& /*srcBodyContentType*/, QByteArray& result, QByteArray& contentType)
{
    return executeGet(path, params, result, contentType);
}
