#include <QFileInfo>
#include "utils/network/tcp_connection_priv.h"
#include "rest/server/rest_server.h"
#include "core/resourcemanagment/resource_pool.h"
#include "utils/common/util.h"
#include "api/serializer/serializer.h"
#include "get_statistics.h"
#include "recorder/storage_manager.h"
#include "utils/common/performance.h"

int QnGetStatisticsHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& resultByteArray)
{
    QnStorageManager::StorageMap storages = qnStorageMan->getAllStorages();
    QString result;
    result.append("<root>\n");

    
    result.append("<storages>\n");
    foreach(QnStorageResourcePtr storage, storages)
    {
        int usageInPersent = storage->getAvarageWritingUsage() * 100 + 0.5;
        result.append(QString("<storage url=\"%1\" usage=\"%2\" />\n").arg(storage->getUrl()).arg(usageInPersent));
    }
    result.append("</storages>\n");

    result.append("<misc>\n");
    
	// todo: determine CPU params in linux
    result.append(QString("<cpuInfo model=\"%1\" cores=\"%2\" usage=\"%3\" />\n")
		.arg(QnPerformance::cpuBrand())	
		.arg(QnPerformance::cpuCoreCount())	
		.arg(QnPerformance::currentCpuUsage()));
    result.append("</misc>\n");


    result.append("</root>\n");

    resultByteArray = result.toUtf8();

    return CODE_OK;
}

int QnGetStatisticsHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result)
{
    return executeGet(path, params, result);
}

QString QnGetStatisticsHandler::description(TCPSocket* tcpSocket) const
{
    QString rez;
    rez += "Returns server info: CPU usage, HDD usage e.t.c \n";
    return rez;
}
