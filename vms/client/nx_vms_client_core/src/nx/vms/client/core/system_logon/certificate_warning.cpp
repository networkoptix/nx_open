// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "certificate_warning.h"

#include <network/system_helpers.h>
#include <nx/network/socket_common.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/common/html/html.h>
#include <nx/utils/log/log.h>

namespace {

const QString kCertificateLink = "#certificate";
const QString kHelpLink = "#help";

} // namespace

namespace nx::vms::client::core {

QString CertificateWarning::header(
    Reason reason,
    const nx::vms::api::ModuleInformation& target,
    ClientType clientType)
{
    const auto& serverName = target.name;
    const auto& systemName = helpers::getSystemName(target);

    if (clientType == ClientType::desktop)
    {
        switch (reason)
        {
            case Reason::unknownServer:
                return tr("Connecting to %1 for the first time?").arg(systemName);
            case Reason::invalidCertificate:
            case Reason::serverCertificateChanged:
                return tr("Cannot verify the identity of %1").arg(serverName);
            default:
                NX_ASSERT(false, "Unreachable");
                return {};
        }
    }

    switch (reason)
    {
        case Reason::unknownServer:
            return tr("Trust this server?");
        case Reason::invalidCertificate:
            return tr("Cannot verify the identity of %1").arg(serverName);
        case Reason::serverCertificateChanged:
            return tr("Trust this server?");
        default:
            NX_ASSERT(false, "Unreachable");
            return {};
    }
}

QString CertificateWarning::details(
    Reason reason,
    const nx::vms::api::ModuleInformation& target,
    const nx::network::SocketAddress& primaryAddress,
    ClientType clientType)
{
    if (clientType == ClientType::desktop)
    {
        switch (reason)
        {
            case Reason::unknownServer:
                return tr(
                    "Review the %1 to ensure you trust the server you are connecting to.\n"
                    "Read this %2 to learn more about certificate validation.",
                    "%1 is <certificate details> link, "
                    "%2 is <help article> link")
                    .arg(
                        common::html::localLink(tr("certificate details"), kCertificateLink),
                        common::html::localLink(tr("help article"), kHelpLink));

            case Reason::invalidCertificate:
            case Reason::serverCertificateChanged:
                return tr(
                    "This might be due to an expired server certificate or someone trying "
                    "to impersonate %1 to steal your personal information.\n"
                    "You can view %2 or read this %3 to learn more about the current problem.",
                    "%1 is the system name, "
                    "%2 is <the server's certificate> link, "
                    "%3 is <help article> link")
                    .arg(
                        helpers::getSystemName(target),
                        common::html::localLink(tr("the server's certificate"), kCertificateLink),
                        common::html::localLink(tr("help article"), kHelpLink));

            default:
                NX_ASSERT(false, "Unreachable");
                return {};
        }
    }

    QString details;
    switch (reason)
    {
        case Reason::unknownServer:
            details = tr("You attempted to connect to this Server, but it presented a certificate "
                "that cannot be verified automatically.");
            break;
        case Reason::invalidCertificate:
            details = tr("Someone may be impersonating this Server to steal your personal "
                "information.");
            break;
        case Reason::serverCertificateChanged:
            details = tr("You attempted to connect to this Server but the Server's certificate has "
                "changed.");
            break;
        default:
            NX_ASSERT(false, "Unreachable");
            break;
    }

    QStringList knownData;
    const static QString kTemplate("<b>%1</b> %2");

    if (!target.systemName.isEmpty())
        knownData <<  kTemplate.arg(tr("System:"), helpers::getSystemName(target));

    QString serverStr;
    if (target.name.isEmpty())
        serverStr = QString::fromStdString(primaryAddress.address.toString());
    else if (primaryAddress.isNull())
        serverStr = target.name;
    else
        serverStr = nx::format("%1 (%2)", target.name, primaryAddress.address);

    if (!serverStr.isEmpty())
        knownData << kTemplate.arg(tr("Server:"), serverStr);

    knownData << kTemplate.arg(tr("Server ID:"), target.id.toSimpleString());

    const auto targetInfo =
        [clientType, knownData]()
        {
            const auto result = QString("<p style='margin-top: 8px; margin-bottom: 8px;'>%1</p>")
                .arg(knownData.join(common::html::kLineBreak));
            return clientType == ClientType::desktop
                ? result
                : QString("%1%1%2%1").arg(common::html::kLineBreak, result);
        }();

    return targetInfo + details;
}

QString CertificateWarning::advice(Reason reason, ClientType clientType)
{
    if (clientType == ClientType::desktop)
    {
        switch (reason)
        {
            case Reason::unknownServer:
                return tr("This message may be shown multiple times when connecting to a multi-server "
                    "system.");
            case Reason::invalidCertificate:
            case Reason::serverCertificateChanged:
                return {};
            default:
                NX_ASSERT(false, "Unreachable");
                return {};
        }
    }

    switch (reason)
    {
        case Reason::unknownServer:
            return tr("Review the certificate's details to make sure you are connecting to the "
                "correct server.");
        case Reason::invalidCertificate:
            return tr("Do not connect to this Server unless instructed by your VMS administrator.");
        case Reason::serverCertificateChanged:
            return tr("Review the certificate's details to make sure you are connecting to the "
                "correct Server.");
        default:
            NX_ASSERT(false, "Unreachable");
            return {};
    }
}

QString CertificateWarning::invalidCertificateError()
{
    return tr("Server certificate is invalid.");
}

} // namespace nx::vms::client::core
