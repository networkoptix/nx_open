#include <QFile>

#include "ifconfig_rest_handler.h"
#include <nx/network/http/line_splitter.h>
#include <network/tcp_connection_priv.h>
#include <nx/fusion/model_functions.h>
#include "common/common_module.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"
#include "utils/common/app_info.h"
#include <rest/server/rest_connection_processor.h>
#include <common/common_module.h>

class IfconfigReply
{
public:
    bool rebootNeeded;

    IfconfigReply()
        :
        rebootNeeded(false)
    {
    }
};

#define IfconfigReply_Fields (rebootNeeded)


QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (IfconfigReply),
    (json),
    _Fields);


namespace
{
    bool writeNetworSettings(const QnNetworkAddressEntryList& settings)
    {
        static const char ENDL = '\n';
    #ifdef Q_OS_WIN
        // On Windows, this method can only be called for debug by manually modifying the code.
        QFile netSettingsFile(lit("c:/etc/network/interfaces"));
        QFile resolveConfFile(lit("c:/etc/resolv.conf"));
    #else
        QFile netSettingsFile(lit("/etc/network/interfaces"));
        QFile resolveConfFile(lit("/etc/resolv.conf"));
    #endif
        if (!netSettingsFile.open(QFile::WriteOnly | QFile::Truncate))
        {
            qWarning() << "Can't write network settings file";
            return false;
        }

        if (!resolveConfFile.open(QFile::WriteOnly | QFile::Truncate))
        {
            qWarning() << "Can't write network settings file (dns server list)";
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
            if (!value.dns_servers.isEmpty())
            {
                out << "    dns-nameservers " << value.dns_servers << ENDL;

                QTextStream out2(&resolveConfFile);
                for (const auto& server : value.dns_servers.split(L' '))
                    out2 << "nameserver " << server << ENDL;
            }
            for(auto itr = value.extraParams.constBegin(); itr != value.extraParams.constEnd(); ++itr)
                out << "    " << itr.key() << " " << itr.value() << ENDL;
            out << ENDL;
        }

        return true;
    }

    //!Returns \a true, if \a newSettings modify something in \a currentSettings
    bool updateSettings(QnNetworkAddressEntryList& currentSettings, QnNetworkAddressEntryList& newSettings)
    {
        bool modified = false;
        // merge existing data
        for(auto& value :currentSettings)
        {
            for (auto itr = newSettings.begin(); itr != newSettings.end(); ++itr)
            {
                const auto& newValue = *itr;
                if (newValue.name != value.name)
                    continue;
                if (!newValue.ipAddr.isNull())
                {
                    modified |= value.ipAddr != newValue.ipAddr;
                    value.ipAddr = newValue.ipAddr;
                }
                if (!newValue.netMask.isNull())
                {
                    modified |= value.netMask != newValue.netMask;
                    value.netMask = newValue.netMask;
                }
                if (!newValue.gateway.isNull())
                {
                    modified |= value.gateway != newValue.gateway;
                    value.gateway = newValue.gateway;
                }
                if (!newValue.dns_servers.isNull())
                {
                    modified |= value.dns_servers != newValue.dns_servers;
                    value.dns_servers = newValue.dns_servers;
                }
                modified |= value.dhcp != newValue.dhcp;
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

        return modified;
    }


    bool isValidIP(const QString& ipAddr)
    {
        return !QHostAddress(ipAddr).isNull();
    }

    bool isValidMask(const QString& ipAddr)
    {
        if (!isValidIP(ipAddr))
            return false;
        quint32 value = QHostAddress(ipAddr).toIPv4Address();
        value = ~value;
        return (value & (value+1)) == 0;
    }

    quint32 getNetworkAddr(const QString& addrStr, const QString& maskStr)
    {
        quint32 addr = QHostAddress(addrStr).toIPv4Address();
        quint32 mask = QHostAddress(maskStr).toIPv4Address();

        return addr & mask;
    }
}

QnIfConfigRestHandler::QnIfConfigRestHandler()
:
    m_modified(false)
{
}

bool QnIfConfigRestHandler::checkData(const QnNetworkAddressEntryList& newSettings, QString* errString)
{
    for (const auto& value: newSettings)
    {
        if (value.name == "lo")
            continue;

        if (!value.ipAddr.isNull() && !isValidIP(value.ipAddr)) {
            *errString = lit("Invalid IP address for interface '%1'").arg(value.name);
            return false;
        }

        if (!value.netMask.isNull() && !isValidMask(value.netMask)) {
            *errString = lit("Invalid network mask for interface '%1'").arg(value.name);
            return false;
        }

        if (!value.gateway.isEmpty())
        {
            if (!isValidIP(value.gateway)) {
                *errString = lit("Invalid gateway address for interface '%1'").arg(value.name);
                return false;
            }
            if (!value.netMask.isEmpty() && !value.ipAddr.isEmpty())
            {
                if (getNetworkAddr(value.ipAddr, value.netMask) != getNetworkAddr(value.gateway, value.netMask)) {
                    *errString = lit("IP address and gateway belong to different networks for interface '%1'").arg(value.name);
                    return false;
                }
            }
        }

        for (const QString& dns: value.dns_servers.split(L' ')) {
            if (!dns.isEmpty() && !isValidIP(dns)) {
                *errString = tr("Invalid dns server for interface '%1'").arg(value.name);
                return false;
            }
        }

        if ((value.ipAddr.isEmpty() || value.netMask.isEmpty()) && !value.dhcp) {
            *errString = tr("IP address or network mask for interface '%1' is empty.").arg(value.name);
            return false;
        }
    }
    return true;
}

int QnIfConfigRestHandler::executePost(
    const QString& /*path*/,
    const QnRequestParams& /*params*/,
    const QByteArray &body,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    const auto& resPool = owner->commonModule()->resourcePool();
    const auto& moduleGuid = owner->commonModule()->moduleGUID();
    QnMediaServerResourcePtr mServer = resPool->getResourceById<QnMediaServerResource>(moduleGuid);
    if (!mServer) {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Internal server error"));
        return CODE_OK;
    }
    if (!(mServer->getServerFlags() & Qn::SF_IfListCtrl)) {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("This server doesn't support interface list control"));
        return CODE_OK;
    }

    bool ok = false;
    QnNetworkAddressEntryList currentSettings = systemNetworkAddressEntryList(&ok, /* addFromConfig */ true);
    if (!ok)
    {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Can't read network settings file"));
        return CODE_OK;
    }

    QnNetworkAddressEntryList newSettings;
    if (!QJson::deserialize(body, &newSettings))
    {
        result.setError(QnJsonRestResult::InvalidParameter, lit("Invalid message body format"));
        return CODE_OK;
    }

    m_modified = updateSettings(currentSettings, newSettings);

    QString errString;
    if (!checkData(currentSettings, &errString)) {
        result.setError(QnJsonRestResult::InvalidParameter, errString);
        return CODE_OK;
    }

    if (!writeNetworSettings(currentSettings)) {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Can't write network settings file"));
    }

    if (m_modified)
    {
        IfconfigReply reply;
#ifdef Q_OS_WIN
        reply.rebootNeeded = false;
#else
        reply.rebootNeeded = true;
#endif
        result.setReply(reply);
    }

    return CODE_OK;
}

void QnIfConfigRestHandler::afterExecute(const QString &path, const QnRequestParamList &params, const QByteArray& body, const QnRestConnectionProcessor* owner)
{
    Q_UNUSED(path);
    Q_UNUSED(params);
    Q_UNUSED(owner);

    QnJsonRestResult reply;
    if (!QJson::deserialize(body, &reply) || reply.error !=  QnJsonRestResult::NoError)
        return;

    bool ok = false;
    QnNetworkAddressEntryList currentSettings = systemNetworkAddressEntryList(&ok, /* addFromConfig */ true);
    if (!ok)
        return;

    if (!m_modified)
        return;

#ifndef Q_OS_WIN
    if(QnAppInfo::isBpi() || QnAppInfo::isNx1())
    {
        //network restart on nx1 sometimes fails, so rebooting
        if( system("/sbin/reboot") != 0 )
            qWarning() << "Failed to reboot";
    }
    else
    {
        if (system("/etc/init.d/networking restart") != 0)
            qWarning() << "Failed to restart networking service";
        for(const auto& value: currentSettings)
            if (system(lit("ifconfig %1 up").arg(value.name).toLatin1().data()) != 0)
                qWarning() << "Failed to restart network interface " << value.name;
    }
#endif
}
