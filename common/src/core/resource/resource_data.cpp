#include "resource_data.h"

#include <core/ptz/ptz_mapper.h>
#include <utils/common/json.h>

QnPtzMapperPtr QnResourceData::ptzMapper() {
    return m_valueByKey.value(lit("ptzMapper")).value<QnPtzMapperPtr>();
}

bool deserialize(const QJsonValue &value, QnResourceData *target) {
    if(value.type() == QJsonValue::Null) {
        /* That's null data. */
        *target = QnResourceData();
        return true;
    }

    QJsonObject map;
    if(!QJson::deserialize(value, &map))
        return false;

    QnResourceData result;
    for(QJsonObject::const_iterator pos = map.begin(); pos != map.end(); pos++) {
        if(pos.key() == lit("ptzMapper")) {
            QnPtzMapperPtr mapper;
            if(!QJson::deserialize(pos.value(), &mapper))
                return false;

            result.setValue(pos.key(), mapper);
        } else {
            result.setValue(pos.key(), pos.value().toString());
        }
    }
    
    *target = result;
    return true;
}





















