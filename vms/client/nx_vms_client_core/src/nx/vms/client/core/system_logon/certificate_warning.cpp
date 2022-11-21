// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "certificate_warning.h"

#include <network/system_helpers.h>
#include <nx/network/socket_common.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/common/html/html.h>
#include <nx/utils/log/log.h>
#include <nx/utils/string.h>

namespace {

const QString kCertificateLink = "#certificate";
const QString kHelpLink = "#help";
constexpr auto kMaxDisplayNameLength = 20;

} // namespace

namespace nx::vms::client::core {

QString CertificateWarning::header(
    Reason reason,
    const nx::vms::api::ModuleInformation& target)
{
    switch (reason)
    {
        case Reason::unknownServer:
            return tr("Connecting to %1 for the first time?")
                .arg(nx::utils::elideString(helpers::getSystemName(target), kMaxDisplayNameLength));
        case Reason::invalidCertificate:
        case Reason::serverCertificateChanged:
            return tr("Cannot verify the identity of %1")
                .arg(nx::utils::elideString(target.name, kMaxDisplayNameLength));
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
    static const auto kCerteficateDetailsText = tr("certificate details");
    static const auto kHelpArticleLink = common::html::localLink(tr("help article"), kHelpLink);
    switch (reason)
    {
        case Reason::unknownServer:
        {
            const auto certificateDetails = clientType == ClientType::mobile
                ? kCerteficateDetailsText
                : common::html::localLink(kCerteficateDetailsText, kCertificateLink);
            const auto extraDetails = clientType == ClientType::mobile
                ? QString()
                : "\n" + tr("Read this %1 to learn more about certificate validation.",
                    "%1 is <help article> link").arg(kHelpArticleLink);
            return tr(
                "Review the %1 to ensure you trust the server you are connecting to.%2",
                "%1 is <certificate details> link, %2 are possible extra details")
                .arg(certificateDetails, extraDetails);
        }
        case Reason::invalidCertificate:
        case Reason::serverCertificateChanged:
        {
            const auto certificateLink = common::html::localLink(
                tr("the server's certificate"), kCertificateLink);
            const auto helpArticleLink = common::html::localLink(tr("help article"), kHelpLink);
            const auto extraDetails = clientType == ClientType::mobile
                ? QString()
                : "\n" + tr ("You can view %1 or read this %2 to learn more "
                    "about the current problem.",
                    "%1 is <the server's certificate> link, %2 is <help article> link")
                    .arg(certificateLink, helpArticleLink);

            return tr(
                "This might be due to an expired server certificate or someone trying "
                "to impersonate %1 to steal your personal information.%2",
                "%1 is the system name, %2 are possible extra details")
                    .arg(helpers::getSystemName(target), extraDetails);
        }
        default:
            NX_ASSERT(false, "Unreachable");
            return {};
    }
}

QString CertificateWarning::advice(Reason reason, ClientType clientType)
{
    switch (reason)
    {
        case Reason::unknownServer:
            return tr("This message may be shown multiple times when connecting to a multi-server "
                "system.");
        case Reason::invalidCertificate:
        case Reason::serverCertificateChanged:
            return clientType == ClientType::mobile
                ? tr("To learn more about the current problem view the server's certificate:")
                : QString();
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
