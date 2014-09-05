#include "update_process.h"

#include <core/resource/media_server_resource.h>

QnPeerUpdateInformation::QnPeerUpdateInformation(const QnMediaServerResourcePtr &server /*= QnMediaServerResourcePtr()*/):
    server(server),
    state(UpdateUnknown),
    updateInformation(0),
    progress(0)
{
    if (server)
        sourceVersion = server->getVersion();
}
