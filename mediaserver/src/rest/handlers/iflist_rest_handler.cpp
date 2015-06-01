#include <QFile>

#include "iflist_rest_handler.h"
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <utils/network/tcp_connection_priv.h>
#include <utils/network/module_information.h>
#include <utils/common/model_functions.h>
#include <common/common_module.h>
#include "utils/network/http/linesplitter.h"

struct QnNetworkAddressEntry
{
    QnNetworkAddressEntry(): dhcp(false) {}

    QString name;
    QString ipAddr;
    QString netMask;
    QString mac;
    bool dhcp;
}; 
typedef QVector<QnNetworkAddressEntry> QnNetworkAddressEntryList;

#define QnNetworkAddressEntry_Fields (name)(ipAddr)(netMask)(mac)(dhcp)
QN_FUSION_DECLARE_FUNCTIONS(QnNetworkAddressEntry, (json)) 
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnNetworkAddressEntry, (json), QnNetworkAddressEntry_Fields)


bool readDHCPState(QnNetworkAddressEntryList& entryList)
{
    QFile netSettings(lit("/etc/network/interfaces"));
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
    while( lineSplitter.parseByLines( nx_http::ConstBufferRefType(data, dataOffset), &line, &bytesRead) )
    {
        dataOffset += bytesRead;
        for (auto& entry: entryList) {
            const auto& data = line.toByteArrayWithRawData();
            QByteArray pattern = QByteArray("iface ").append(entry.name.toLatin1());
            if (!entry.name.isEmpty() && data.startsWith(pattern))
                entry.dhcp = data.contains("dhcp");
        }
    }

    return true;
}

int QnIfListRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) 
{
    Q_UNUSED(path)

    QnNetworkAddressEntryList entryList;
    for (const QnInterfaceAndAddr& iface: getAllIPv4Interfaces()) 
    {
        QnNetworkAddressEntry entry;
        entry.name = iface.netIf.name();
        entry.ipAddr = iface.address.toString();
        entry.netMask = iface.netMask.toString().toLatin1();
        entry.mac = iface.netIf.hardwareAddress().toLatin1();
        entryList.push_back(std::move(entry));
    }
    readDHCPState(entryList);

    result.setReply(entryList);

    return CODE_OK;
}
