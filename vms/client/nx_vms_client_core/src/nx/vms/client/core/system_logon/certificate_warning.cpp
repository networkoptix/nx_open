// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "certificate_warning.h"

#include <network/system_helpers.h>
#include <nx/network/socket_common.h>
#include <nx/utils/log/log.h>
#include <nx/utils/string.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/common/html/html.h>

namespace {

const QString kCertificateLink = "#certificate";
const QString kHelpLink = "#help";
constexpr auto kMaxDisplayNameLength = 20;

} // namespace

namespace nx::vms::client::core {

QString CertificateWarning::header(
    Reason reason,
    const nx::vms::api::ModuleInformation& info,
    int numberOfServers)
{
    if (!NX_ASSERT(numberOfServers > 0))
        return {};

    switch (reason)
    {
        case Reason::unknownServer:
            return tr("Connecting to %1 for the first time?")
                .arg(nx::utils::elideString(helpers::getSystemName(info), kMaxDisplayNameLength));
        case Reason::invalidCertificate:
        case Reason::serverCertificateChanged:
            return tr("Cannot verify the identity of %n servers", "", numberOfServers);
        default:
            NX_ASSERT(false, "Unreachable");
            return {};
    }
}

QString CertificateWarning::details(Reason reason, ClientType clientType)
{
    switch (reason)
    {
        case Reason::unknownServer:
        {
            return tr("Review the certificates of the servers from this system.");
        }
        case Reason::invalidCertificate:
        case Reason::serverCertificateChanged:
        {
            return tr(
                "This might be due to an expired server certificate or an invalid certificate. "
                "Contact your system administrator for further investigation.");
        }
        default:
            NX_ASSERT(false, "Unreachable");
            return {};
    }
}

QString CertificateWarning::advice(Reason reason, ClientType clientType)
{
    static const auto kHelpArticleLink = common::html::localLink(tr("help article"), kHelpLink);
    switch (reason)
    {
        case Reason::unknownServer:
        {
            return clientType == ClientType::mobile
                ? QString()
                : tr("Read this %1 to learn more about certificate validation.",
                    "%1 is <help article> link").arg(kHelpArticleLink);
        }
        case Reason::invalidCertificate:
        case Reason::serverCertificateChanged:
        {
            return clientType == ClientType::mobile
                ? QString()
                : tr("To learn more about the current problem read this %1.",
                     "%1 is <help article> link").arg(kHelpArticleLink);
        }
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
