#include <QFile>

#include "ifconfig_rest_handler.h"
#include "network_address_entry.h"
#include "utils/network/http/linesplitter.h"
#include <utils/network/tcp_connection_priv.h>
#include "common/common_module.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"

QnNetworkAddressEntryList readNetworSettings(bool* ok)
{
    QnNetworkAddressEntryList result;
#ifdef Q_OS_WIN
    QFile netSettings(lit("c:/etc/network/interfaces"));
#else
    QFile netSettings(lit("/etc/network/interfaces"));
#endif
    *ok = true;
    if (!netSettings.open(QFile::ReadOnly))
    {
        qWarning() << "Can't read network settings file";
        *ok = false;
        return result;
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
        {
            result.push_back(QnNetworkAddressEntry());
            result.last().name = worlds[1].trimmed();
            result.last().dhcp = data.contains("dhcp");
        }
        else {
            if (result.isEmpty())
                continue;

            if (data.startsWith("address"))
                result.last().ipAddr = worlds[1].trimmed();
            else if (data.startsWith("netmask"))
                result.last().netMask = worlds[1].trimmed();
            else if (data.startsWith("gateway"))
                result.last().gateway = worlds[1].trimmed();
            else if (data.startsWith("auto"))
                ;
            else
                result.last().extraParams.insert(worlds[0], worlds[1]);
        }
    }
    
    return result;
}

bool writeNetworSettings(const QnNetworkAddressEntryList& settings)
{
    static const char ENDL = '\n';
#ifdef Q_OS_WIN
    QFile netSettingsFile(lit("c:/etc/network/interfaces"));
#else
    QFile netSettingsFile(lit("/etc/network/interfaces"));
#endif
    if (!netSettingsFile.open(QFile::WriteOnly | QFile::Truncate))
    {
        qWarning() << "Can't write network settings file";
        return false;
    }
    
    QTextStream out(&netSettingsFile);
    for(const auto& value: settings)
    {
        out << "auto " << value.name << ENDL;
        out << "iface " << value.name << " inet ";
        if (value.name == "lo")
            out << "loopback" << ENDL;
        else if (value.dhcp)
            out << "dhcp" << ENDL;
        else
            out << "static" << ENDL;
        if (!value.ipAddr.isEmpty())
            out << "    address " << value.ipAddr << ENDL;
        if (!value.netMask.isEmpty())
            out << "    netmask " << value.netMask << ENDL;
        if (!value.gateway.isEmpty())
            out << "    gateway " << value.gateway << ENDL;
        for(auto itr = value.extraParams.constBegin(); itr != value.extraParams.constEnd(); ++itr)
            out << "    " << itr.key() << " " << itr.value() << ENDL;
        out << ENDL;
    }
    return true;
}

void updateSettings(QnNetworkAddressEntryList& currentSettings, QnNetworkAddressEntryList& newSettings)
{
    // merge existing data
    for(auto& value :currentSettings) 
    {
        for (auto itr = newSettings.begin(); itr != newSettings.end(); ++itr)
        {
            const auto& newValue = *itr;
            if (newValue.name != value.name)
                continue;
            if (!newValue.ipAddr.isNull())
                value.ipAddr = newValue.ipAddr;
            if (!newValue.netMask.isNull())
                value.netMask = newValue.netMask;
            if (!newValue.gateway.isNull())
                value.gateway = newValue.gateway;
            value.dhcp = newValue.dhcp;
            newSettings.erase(itr);
            break;
        }
    }
#if 0
    // add new data
    for (const auto& newValue :newSettings)
        currentSettings.push_back(newValue);
#endif
}

int QnIfConfigRestHandler::executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor*)
{
    Q_UNUSED(path)
    Q_UNUSED(params)

    QnMediaServerResourcePtr mServer = qnResPool->getResourceById<QnMediaServerResource>(qnCommon->moduleGUID());
    if (!mServer) {
        result.setError(QnJsonRestResult::CantProcessRequest);
        result.setErrorString(lit("Internal server error"));
        return CODE_OK;
    }
    if (!(mServer->getServerFlags() & Qn::SF_IfListCtrl)) {
        result.setError(QnJsonRestResult::CantProcessRequest);
        result.setErrorString(lit("This server doesn't support interface list control"));
        return CODE_OK;
    }

    bool ok = false;
    QnNetworkAddressEntryList currentSettings = readNetworSettings(&ok);
    if (!ok) 
    {
        result.setError(QnJsonRestResult::CantProcessRequest);
        result.setErrorString(lit("Can't read network settings file"));
        return CODE_OK;
    }
    
    QnNetworkAddressEntryList newSettings;
    if (!QJson::deserialize(body, &newSettings)) 
    {
        result.setError(QnJsonRestResult::InvalidParameter);
        result.setErrorString(lit("Invalid message body format"));
        return CODE_OK;
    }

    updateSettings(currentSettings, newSettings);
    if (!writeNetworSettings(currentSettings)) {
        result.setError(QnJsonRestResult::CantProcessRequest);
        result.setErrorString(lit("Can't write network settings file"));
    }
#ifndef Q_OS_WIN
    if (system("/etc/init.d/networking restart") != 0)
        qWarning() << "Failed to restart networking service";
    for(const auto& value: currentSettings)
        if (system(lit("ifconfig %1 up").arg(value.name).toLatin1().data()) != 0)
            qWarning() << "Failed to restart network interface " << value.name;
#endif
    return CODE_OK;
}
