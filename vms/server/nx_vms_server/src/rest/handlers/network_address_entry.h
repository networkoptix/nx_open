#ifndef __NETWORK_ADDRESS_ENTRY_H
#define __NETWORK_ADDRESS_ENTRY_H

#include <nx/fusion/model_functions.h>

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
    QString dns_servers; // it can be several IP via space delimiter
}; 
typedef QVector<QnNetworkAddressEntry> QnNetworkAddressEntryList;

#define QnNetworkAddressEntry_Fields (name)(ipAddr)(netMask)(mac)(gateway)(dhcp)(extraParams)(dns_servers)
QN_FUSION_DECLARE_FUNCTIONS(QnNetworkAddressEntry, (json)) 

QnNetworkAddressEntryList systemNetworkAddressEntryList(bool* isOk = nullptr, bool addFromConfig = false);

#endif
