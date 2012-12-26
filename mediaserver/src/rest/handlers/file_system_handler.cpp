#include "file_system_handler.h"

#include <QtCore/QFileInfo>

#include <api/serializer/serializer.h>
#include <core/resource_managment/resource_pool.h>
#include <platform/platform_abstraction.h>
#include <recorder/storage_manager.h>
#include <rest/server/rest_server.h>
#include <utils/common/util.h>
#include <utils/network/tcp_connection_priv.h>
#include <utils/xml/light_xml_builder.h>

QnFileSystemHandler::QnFileSystemHandler() {
    m_monitor = qnPlatform->monitor();
}

int QnFileSystemHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType) {
    Q_UNUSED(path)
    Q_UNUSED(contentType)
    QString pathStr;

    for (int i = 0; i < params.size(); ++i)
    {
        if (params[i].first == "path")
        {
            pathStr = params[i].second;
            break;
        }
    }

    if (pathStr.isEmpty()) {
        const QnStorageManager::StorageMap storages = qnStorageMan->getAllStorages();

        QnLightXmlBuilder xml;
        xml.beginSection("partitions");
        foreach(const QnPlatformMonitor::PartitionSpace &space, m_monitor->totalPartitionSpaceInfo()) {
            xml.beginSection("partition");
            xml.writeValue("url", space.partition);
            xml.writeValue("free", space.freeBytes);
            xml.writeValue("size", space.sizeBytes);

            {   //storages at this partition
                xml.beginSection("storages");
                for(QnStorageManager::StorageMap::const_iterator itr = storages.constBegin(); itr != storages.constEnd(); ++itr)
                {
                    QString root = itr.value()->getUrl();
                    if (m_monitor->partitionByPath(root) == space.partition) {
                    //if (root.startsWith(space.partition)) {
                        xml.beginSection("storage");
                        xml.writeValue("url", root);
                        xml.writeValue("used", itr.value()->getWritedSpace());
                        xml.endSection();
                    }
                }
                xml.endSection();
            }

            xml.endSection();
        }
        xml.endSection();
        if (!xml.isValid())
            return CODE_INTERNAL_ERROR;
        result.append(xml.body());

    } else {
        QnStorageResourcePtr storage = qnStorageMan->getStorageByUrl(pathStr);
        if (storage == 0)
            storage = QnStorageResourcePtr(QnStoragePluginFactory::instance()->createStorage(pathStr, false));

        int prefixPos = pathStr.indexOf("://");
        QString prefix;
        if (prefixPos != -1)
            prefix = pathStr.left(prefixPos+3);
        if (storage == 0)
        {
            result.append("<root>\n");
            result.append(QString("Unknown storage plugin '%1'").arg(prefix));
            result.append("</root>\n");
            return CODE_OK;
        }

        pathStr = pathStr.mid(prefix.length());
        storage->setUrl(pathStr);
        QString rezStr;
        if (storage->isStorageAvailableForWriting()) {
            rezStr.append("<freeSpace>\n");
            rezStr.append(QByteArray::number(storage->getFreeSpace()));
            rezStr.append("</freeSpace>\n");
            rezStr.append("<usedSpace>\n");
            rezStr.append(QByteArray::number(storage->getWritedSpace()));
            rezStr.append("</usedSpace>\n");
        }
        else {
            rezStr = "-1";
        }

        result.append("<root>\n");
        result.append(rezStr);
        result.append("</root>\n");
    }

    return CODE_OK;
}

int QnFileSystemHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(body)
    return executeGet(path, params, result, contentType);
}

QString QnFileSystemHandler::description(TCPSocket* tcpSocket) const
{
    Q_UNUSED(tcpSocket)
    QString rez;
    rez += "Returns storage free space and current usage in bytes. if specified folder can not be used for writing or not available returns -1.\n";
    rez += "<BR>Param <b>path</b> - Folder.";
    rez += "<BR><b>Return</b> XML - free space in bytes or -1";
    return rez;
}
