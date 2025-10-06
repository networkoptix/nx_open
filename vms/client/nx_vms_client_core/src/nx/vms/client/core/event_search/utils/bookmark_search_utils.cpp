// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_search_utils.h"

#include <core/resource/media_server_resource.h>
#include <nx/utils/software_version.h>
#include <nx/utils/string.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/system_context.h>

namespace nx::vms::client::core {

QString formatFilterText(const QString& text)
{
    const auto systemContext = appContext()->currentSystemContext();
    const auto server = systemContext ? systemContext->currentServer() : QnMediaServerResourcePtr{};
    if (server && server->getVersion() < nx::utils::SoftwareVersion(6, 1))
        return text;

    static const QStringList delimiters{"OR", "AND"};
    return nx::utils::quoteDelimitedTokens(text, delimiters);
}

} // namespace nx::vms::client::core
