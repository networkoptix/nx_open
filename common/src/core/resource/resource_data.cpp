#include "resource_data.h"

#include <core/ptz/ptz_mapper.h>
#include <utils/serialization/json.h>

QnPtzMapperPtr QnResourceData::ptzMapper() {
    return m_valueByKey.value(lit("ptzMapper")).value<QnPtzMapperPtr>();
}

bool deserialize(QnJsonContext *ctx, const QJsonValue &value, QnResourceData *target) {
    if(value.type() == QJsonValue::Null) {
        /* That's null data. */
        *target = QnResourceData();
        return true;
    }

    QJsonObject map;
    if(!QJson::deserialize(ctx, value, &map))
        return false;

    QnResourceData result;
    for(QJsonObject::const_iterator pos = map.begin(); pos != map.end(); pos++) {
        if(pos.key() == lit("ptzMapper")) {
            QnPtzMapperPtr mapper;
            if(!QJson::deserialize(ctx, pos.value(), &mapper))
                return false;

            result.setValue(pos.key(), mapper);
        } else {
            result.setValue(pos.key(), pos.value().toVariant());
        }
    }
    
    *target = result;
    return true;
}





















