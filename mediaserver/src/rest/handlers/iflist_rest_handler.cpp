#include "iflist_rest_handler.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <utils/network/tcp_connection_priv.h>
#include <utils/network/module_information.h>
#include <utils/common/model_functions.h>
#include <common/common_module.h>

struct QnNetworkAddressEntry
{
    QnNetworkAddressEntry(): dhcp(false) {}

    QString ipAddr;
    QString netMask;
    QString mac;
    bool dhcp;
}; 
typedef QVector<QnNetworkAddressEntry> QnNetworkAddressEntryList;

#define QnNetworkAddressEntry_Fields (ipAddr)(netMask)(mac)(dhcp)
QN_FUSION_DECLARE_FUNCTIONS(QnNetworkAddressEntry, (json)) 
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnNetworkAddressEntry, (json), QnNetworkAddressEntry_Fields)


int QnIfListRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) 
{
    Q_UNUSED(path)

    QnNetworkAddressEntryList entryList;
    for (const QnInterfaceAndAddr& iface: getAllIPv4Interfaces()) 
    {
        QnNetworkAddressEntry entry;
        entry.ipAddr = iface.address.toString();
        entry.netMask = iface.netMask.toString().toLatin1();
        entry.mac = iface.netIf.hardwareAddress().toLatin1();
        entryList.push_back(std::move(entry));
    }
    
    result.setReply(entryList);

    return CODE_OK;
}
