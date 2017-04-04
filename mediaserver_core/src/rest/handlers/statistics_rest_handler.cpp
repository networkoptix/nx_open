#include "statistics_rest_handler.h"

#include "utils/common/util.h"
#include "network/tcp_connection_priv.h"
#include "platform/platform_abstraction.h"
#include "api/model/statistics_reply.h"
#include <rest/server/rest_connection_processor.h>
#include <common/common_module.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/resource_pool.h>


QnStatisticsRestHandler::QnStatisticsRestHandler(): QnJsonRestHandler() {
    m_monitor = qnPlatform->monitor();
}

QnStatisticsRestHandler::~QnStatisticsRestHandler() {
    return;
}

int QnStatisticsRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& /*params*/,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    if (!owner->resourceAccessManager()->hasPermission(
            owner->accessRights(),
            owner->resourcePool()->getResourceById(owner->commonModule()->moduleGUID()),
            Qn::ViewContentPermission))
    {
        return nx_http::StatusCode::forbidden;
    }

    QnStatisticsReply reply;
    reply.updatePeriod = m_monitor->updatePeriod();
    reply.uptimeMs = m_monitor->upTimeMs();

    QnStatisticsDataList cpuInfo;
    cpuInfo.append(QnStatisticsDataItem(lit("CPU"), m_monitor->totalCpuUsage(), Qn::StatisticsCPU));
    //cpuInfo.append(QnStatisticsDataItem(lit("CPU-CORES"), QThread::idealThreadCount(), Qn::StatisticsCPU));
    reply.statistics << cpuInfo;

    QnStatisticsDataList memory;
    memory.append(QnStatisticsDataItem(lit("RAM"), m_monitor->totalRamUsage(), Qn::StatisticsRAM));
    reply.statistics << memory;

    QnStatisticsDataList storages;
    for(const QnPlatformMonitor::HddLoad &hddLoad: m_monitor->totalHddLoad()) {
        storages.append(QnStatisticsDataItem(hddLoad.hdd.partitions, hddLoad.load, Qn::StatisticsHDD));
    }
    reply.statistics << storages;

    QnStatisticsDataList network;
    for(const QnPlatformMonitor::NetworkLoad &networkLoad: m_monitor->totalNetworkLoad())
    {
        qint64 bytesIn = networkLoad.bytesPerSecIn;
        qint64 bytesOut = networkLoad.bytesPerSecOut;
        qint64 bytesMax = networkLoad.bytesPerSecMax;
        network.append(QnStatisticsDataItem(networkLoad.interfaceName, static_cast<qreal>(qMax(bytesIn, bytesOut)) / bytesMax, Qn::StatisticsNETWORK, static_cast<int>(networkLoad.type)));
    }
    reply.statistics << network;

    result.setReply(reply);

    return CODE_OK;
}
