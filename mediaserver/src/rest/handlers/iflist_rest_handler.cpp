#include <QFile>

#include "iflist_rest_handler.h"
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <utils/network/tcp_connection_priv.h>
#include <common/common_module.h>
#include "utils/network/http/linesplitter.h"
#include "network_address_entry.h"

bool readExtraFields(QnNetworkAddressEntryList& entryList)
{
#ifdef Q_OS_WIN
    QFile netSettings(lit("c:/etc/network/interfaces"));
#else
    QFile netSettings(lit("/etc/network/interfaces"));
#endif
    if (!netSettings.open(QFile::ReadOnly))
    {
        qWarning() << "Can't read network settings file to determine DHCP settings";
        return false;
    }
    const QByteArray data = netSettings.readAll();

    nx_http::LineSplitter lineSplitter;
    QnByteArrayConstRef line;
    size_t bytesRead = 0;
    size_t dataOffset = 0;
    QByteArray lastIfName;
    while( lineSplitter.parseByLines( nx_http::ConstBufferRefType(data, dataOffset), &line, &bytesRead) )
    {
        dataOffset += bytesRead;

        QByteArray data = line.toByteArrayWithRawData().trimmed();
        QList<QByteArray> worlds = data.split(' ');
        if (worlds.size() < 2)
            continue;

        if (data.startsWith("iface")) 
            lastIfName = worlds[1].trimmed();

        for (auto& entry: entryList) 
        {
            if (entry.name == lastIfName) 
            {
                if (data.startsWith("iface")) 
                    entry.dhcp = data.contains("dhcp");
                else if (data.startsWith("netmask"))
                    entry.netMask = worlds[1].trimmed();
                else if (data.startsWith("gateway"))
                    entry.gateway = worlds[1].trimmed();
                else if (data.startsWith("dns-nameservers"))
                    entry.dns_servers = data.mid(worlds[0].length() + 1);
            }
        }
    }

    return true;
}

#include <utils/common/log.h>
#include <utils/common/app_info.h>

int QnIfListRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) 
{
    Q_UNUSED(path)
    Q_UNUSED(params)

    QnNetworkAddressEntryList entryList;

    const bool allInterfaces = (QnAppInfo::armBox() == "bpi");
    for (const QnInterfaceAndAddr& iface: getAllIPv4Interfaces(allInterfaces)) 
    {
        static const QChar kColon = ':';
        if (allInterfaces && iface.name.contains(kColon))
        {
            NX_LOG(lit("Skipping interface" ) + iface.name, cl_logALWAYS);
            continue;
        }

        NX_LOG(lit("Reading extra interface data " ) + iface.name, cl_logALWAYS);

        QnNetworkAddressEntry entry;
        entry.name = iface.netIf.name();
        entry.ipAddr = iface.address.toString();
        entry.netMask = iface.netMask.toString().toLatin1();
        entry.mac = iface.netIf.hardwareAddress().toLatin1();
        entryList.push_back(std::move(entry));
    }
    readExtraFields(entryList);

    result.setReply(entryList);

    return CODE_OK;
}
