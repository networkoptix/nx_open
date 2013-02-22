#include "storage_space_data.h"

#include <utils/common/json.h>

void serialize(const QnStorageSpaceData &value, QVariant *target) {
	QVariantMap result;
	QJson::serialize(value.path, "path", &result);
	QJson::serialize(value.storageId, "storageId", &result);
	QJson::serialize(value.totalSpace, "totalSpace", &result);
	QJson::serialize(value.freeSpace, "freeSpace", &result);
	*target = result;
}

bool deserialize(const QVariant &value, QnStorageSpaceData *target) {
	if(value.type() != QVariant::Map)
		return false;
	QVariantMap map = value.toMap();

	QnStorageSpaceData result;
	if(
		!QJson::deserialize(map, "path", &result.path) || 
		!QJson::deserialize(map, "storageId", &result.storageId) ||
		!QJson::deserialize(map, "totalSpace", &result.totalSpace) || 
		!QJson::deserialize(map, "freeSpace", &result.freeSpace)
		) {
			return false;
	}

	*target = result;
	return true;
}
