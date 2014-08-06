#include "resource_data.h"

#include <cassert>

#include <core/ptz/ptz_mapper.h>
#include <utils/serialization/json_functions.h>


class QnResourceDataJsonSerializer: public QnJsonSerializer {
public:
    QnResourceDataJsonSerializer():
        QnJsonSerializer(qMetaTypeId<QnResourceData>())
    {
        registerKey<QnPtzMapperPtr>(lit("ptzMapper"));
        registerKey<Qn::PtzCapabilities>(lit("ptzCapabilities"));
        registerKey<Qn::PtzTraits>(lit("ptzTraits"));
        registerKey<QStringList>(lit("vistaFocusDevices"));
    }

protected:
    virtual void serializeInternal(QnJsonContext *, const void *, QJsonValue *) const override {
        assert(false); /* Not supported for now. */
    }

    virtual bool deserializeInternal(QnJsonContext *ctx, const QJsonValue &value, void *target) const override {
        return deserializeInternal(ctx, value, static_cast<QnResourceData *>(target));
    }

    bool deserializeInternal(QnJsonContext *ctx, const QJsonValue &value, QnResourceData *target) const {
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
            QnResourceData::Data &data = result.m_dataByKey[pos.key()];
            data.json = pos.value();

            QnJsonSerializer *serializer = m_serializerByKey.value(pos.key());
            if(serializer) {
                if(!serializer->deserialize(ctx, data.json, &data.value))
                    return false;
                
                data.type = serializer->type();
            }
        }

        *target = result;
        return true;
    }

private:
    template<class T>
    void registerKey(const QString &key) {
        QnJsonSerializer *serializer = QnJsonSerializer::serializer(qMetaTypeId<T>());
        assert(serializer);
        m_serializerByKey.insert(key, serializer);
    }

private:
    QHash<QString, QnJsonSerializer *> m_serializerByKey;
};

Q_GLOBAL_STATIC(QnResourceDataJsonSerializer, qn_resourceDataJsonSerializer_instance)


bool QnResourceData::value(const QString &key, int type, void *value, const CopyFunction &copyFunction) const {
    auto pos = m_dataByKey.find(key);
    if(pos == m_dataByKey.end())
        return false;

    /* Note: we're using type erasure here so that we don't have to include json
     * headers from this header. Performance penalty is negligible as this 
     * function is only called during resource construction. */

    const Data &data = *pos;
    if(data.type == type) {
        copyFunction(data.value.data(), value);
        return true;
    }

    if(data.type != QMetaType::UnknownType)
        qnWarning("Resource data for key '%1' was requested with a non-standard type '%2'.", key, QMetaType::typeName(type));

    QnJsonSerializer *serializer = QnJsonSerializer::serializer(type);
    assert(serializer);

    QnJsonContext ctx;
    return serializer->deserialize(&ctx, data.json, value);
}

void QnResourceData::add(const QnResourceData &other) {
    if(m_dataByKey.isEmpty()) {
        m_dataByKey = other.m_dataByKey;
    } else {
        for(auto pos = other.m_dataByKey.begin(); pos != other.m_dataByKey.end(); pos++)
            m_dataByKey.insert(pos.key(), pos.value());
    }
}

bool deserialize(QnJsonContext *ctx, const QJsonValue &value, QnResourceData *target) {
    return qn_resourceDataJsonSerializer_instance()->deserialize(ctx, value, target);
}





















