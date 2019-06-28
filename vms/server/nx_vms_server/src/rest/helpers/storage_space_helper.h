#pragma once

#include <api/model/api_model_fwd.h>
#include <api/model/storage_status_reply.h>
#include <QtCore/QList>

class QnMediaServerModule;

namespace nx::rest::helpers {

QnStorageSpaceDataList availableStorages(QnMediaServerModule* serverModule);

} // namespace nx::rest