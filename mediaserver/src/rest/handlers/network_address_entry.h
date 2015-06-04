#ifndef __NETWORK_ADDRESS_ENTRY_H
#define __NETWORK_ADDRESS_ENTRY_H

#include <utils/common/model_functions.h>

struct QnNetworkAddressEntry
{
    QnNetworkAddressEntry(): dhcp(false) {}

    QString name;
    QString ipAddr;
    QString netMask;
    QString mac;
    QString gateway;
    bool dhcp;
    QMap<QString, QString> extraParams;
}; 
typedef QVector<QnNetworkAddressEntry> QnNetworkAddressEntryList;

#define QnNetworkAddressEntry_Fields (name)(ipAddr)(netMask)(mac)(gateway)(dhcp)(extraParams)
QN_FUSION_DECLARE_FUNCTIONS(QnNetworkAddressEntry, (json)) 


#endif
