#pragma once

#include <api/model/storage_status_reply.h>
#include <optional>

namespace nx::update::storage {

using ServerToStorages = std::pair<QnUuid, QnStorageSpaceDataList>;
using ServerToStoragesList = std::vector<ServerToStorages>;

std::optional<QnStorageSpaceData> selectOne(const QnStorageSpaceDataList& candidates);
QList<QnUuid> selectServers(const ServerToStoragesList& serverToStorages);

} // namespace nx::update::storage