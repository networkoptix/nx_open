// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "certificate_warning.h"

#include <network/system_helpers.h>
#include <nx/network/socket_common.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/common/html/html.h>
#include <nx/utils/log/log.h>

namespace nx::vms::client::core {

QString CertificateWarning::header(Reason reason, const QString& serverName)
{
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
    const nx::network::SocketAddress& primaryAddress)
{
    auto result =
        [reason]()
        {
            switch (reason)
            {
                case Reason::unknownServer:
                    return tr(
                        "You attempted to connect to: %1 "
                        "but the Server presented a certificate that is unable to be automatically verified.");
                case Reason::invalidCertificate:
                    return tr("Someone may be impersonating %1 to steal your personal information.");
                case Reason::serverCertificateChanged:
                    return tr(
                        "You attempted to connect to: %1 "
                        "but the Server's certificate has changed.");
                default:
                    NX_ASSERT(false, "Unreachable");
                    return QString();
            }
        }();

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
        QString("<p style='margin-top: 8px; margin-bottom: 8px;'>%1</p>")
            .arg(knownData.join(common::html::kLineBreak));

    // Patch up details string: we don't want to have spaces around target info block.
    const static QString kPlaceholder("%1");
    const auto pattern = result.arg(kPlaceholder);
    const auto substrings = result.split(kPlaceholder);
    if (substrings.size() == 2)
    {
        result = substrings[0].trimmed() + targetInfo + substrings[1].trimmed();
    }
    else
    {
        // Something failed. Perhaps the translation does not meet our expectations.
        result = result.arg(targetInfo);
    }
    return result;
}

QString CertificateWarning::advice(Reason reason)
{
    switch (reason)
    {
        case Reason::unknownServer:
            return tr(
                "Review the certificate's details to make sure "
                "you are connecting to the correct server.");
        case Reason::invalidCertificate:
            return tr("Do not connect to this server unless instructed by your VMS administrator.");
        case Reason::serverCertificateChanged:
            return tr(
                "Review the certificate's details to make sure "
                "you are connecting to the correct server.");
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
