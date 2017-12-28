#include "network_address_entry.h"

#include <QFile>

#include <nx/network/http/line_splitter.h>
#include <nx/network/nettools.h>
#include <utils/common/app_info.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnNetworkAddressEntry, (json), QnNetworkAddressEntry_Fields, (optional, true))

static bool readExtraFields(QnNetworkAddressEntryList& entryList, bool addFromConfig)
{
    #ifdef Q_OS_WIN
        // On Windows, this method can only be called for debug by manually modifying the code.
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

    nx::network::http::LineSplitter lineSplitter;
    QnByteArrayConstRef line;
    size_t bytesRead = 0;
    size_t dataOffset = 0;
    QnNetworkAddressEntry* currentEntry = nullptr;
    while( lineSplitter.parseByLines( nx::network::http::ConstBufferRefType(data, dataOffset), &line, &bytesRead) )
    {
        dataOffset += bytesRead;

        QByteArray data = line.toByteArrayWithRawData().trimmed();
        QList<QByteArray> worlds = data.split(' ');
        if (worlds.size() < 2)
            continue;

        if (data.startsWith("iface"))
        {
            const auto name = worlds[1].trimmed();
            const auto it = std::find_if(entryList.begin(), entryList.end(),
                [&name](const QnNetworkAddressEntry& e) { return e.name == name; });

            if (it != entryList.end())
            {
                currentEntry = &(*it);
            }
            else if (addFromConfig)
            {
                QnNetworkAddressEntry newEntry;
                newEntry.name = name;
                entryList.push_back(std::move(newEntry));
                currentEntry = &entryList.back();
            }
            else
            {
                currentEntry = nullptr;
            }
        }

        if (currentEntry)
        {
            if (data.startsWith("iface"))
                currentEntry->dhcp = data.contains("dhcp");
            else if (data.startsWith("netmask"))
                currentEntry->netMask = worlds[1].trimmed();
            else if (data.startsWith("gateway"))
                currentEntry->gateway = worlds[1].trimmed();
            else if (data.startsWith("dns-nameservers"))
                currentEntry->dns_servers = data.mid(worlds[0].length() + 1);
        }
    }

    return true;
}

QnNetworkAddressEntryList systemNetworkAddressEntryList(bool* isOk, bool addFromConfig)
{
    QnNetworkAddressEntryList entryList;

    const bool allInterfaces = (QnAppInfo::isBpi());
    for (const QnInterfaceAndAddr& iface: getAllIPv4Interfaces(allInterfaces))
    {
        static const QChar kColon = ':';
        if (allInterfaces && iface.name.contains(kColon))
            continue;

        QnNetworkAddressEntry entry;
        entry.name = iface.netIf.name();
        entry.ipAddr = iface.address.toString();
        entry.netMask = iface.netMask.toString().toLatin1();
        entry.mac = iface.netIf.hardwareAddress().toLatin1();
        entryList.push_back(std::move(entry));
    }

    const auto result = readExtraFields(entryList, addFromConfig);
    if (isOk)
        *isOk = result;

    return entryList;
}
