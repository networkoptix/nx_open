#include "resource_data.h"

#include <core/ptz/ptz_mapper.h>

QnPtzMapperPtr QnResourceData::ptzMapper() {
    return m_valueByKey.value(lit("ptzMapper")).value<QnPtzMapperPtr>();
}

bool deserialize(const QVariant &value, QnResourceData *target) {
    if(value.type() == QVariant::Invalid) {
        /* That's null data. */
        *target = QnResourceData();
        return true;
    }

    if(value.type() != QVariant::Map)
        return false;
    QVariantMap map = value.toMap();

    QnResourceData result;
    for(QVariantMap::const_iterator pos = map.begin(); pos != map.end(); pos++) {
        if(pos.key() == lit("ptzMapper")) {
            QnPtzMapperPtr mapper;
            if(!deserialize(pos.value(), &mapper))
                return false;

            result.setValue(pos.key(), mapper);
        } else {
            result.setValue(pos.key(), pos.value().toString());
        }
    }
    
    *target = result;
    return true;
}





















