#include "resource_data.h"
#include "param.h"
#include <cassert>
#include <api/model/api_ioport_data.h>
#include <core/ptz/ptz_mapper.h>
#include <core/ptz/ptz_constants.h>
#include <nx/core/ptz/override.h>
#include <nx/fusion/serialization/json_functions.h>
#include <utils/common/credentials.h>
#include <core/dataprovider/stream_mixer_data.h>
#include <core/resource/resource_data_structures.h>
#include <core/resource/camera_advanced_param.h>

class QnResourceDataJsonSerializer: public QnJsonSerializer {
public:
    QnResourceDataJsonSerializer():
        QnJsonSerializer(qMetaTypeId<QnResourceData>())
    {
        registerKey<QnPtzMapperPtr>(lit("ptzMapper"));
        registerKey<Ptz::Traits>(lit("ptzTraits"));
        registerKey<QStringList>(lit("vistaFocusDevices"));
        registerKey<QnIOPortDataList>(lit("ioSettings"));
        registerKey<QList<nx::vms::common::Credentials>>(
            ResourceDataKey::kPossibleDefaultCredentials);
        registerKey<nx::vms::common::Credentials>(ResourceDataKey::kForcedDefaultCredentials);
        registerKey<QList<QnResourceChannelMapping>>(
            ResourceDataKey::kMultiresourceVideoChannelMapping);
        registerKey<QnHttpConfigureRequestList>(ResourceDataKey::kPreStreamConfigureRequests);
        registerKey<QnBitrateList>(ResourceDataKey::kHighStreamAvailableBitrates);
        registerKey<QnBitrateList>(ResourceDataKey::kLowStreamAvailableBitrates);
        registerKey<QnBounds>(ResourceDataKey::kHighStreamBitrateBounds);
        registerKey<QnBounds>(ResourceDataKey::kLowStreamBitrateBounds);
        registerKey<TwoWayAudioParams>(ResourceDataKey::kTwoWayAudio);
        registerKey<std::vector<QnCameraAdvancedParameterOverload>>(
            ResourceDataKey::kAdvancedParameterOverloads);
        registerKey<nx::core::ptz::Override>(nx::core::ptz::Override::kPtzOverrideKey);
    }

protected:
    virtual void serializeInternal(QnJsonContext *, const void *, QJsonValue *) const override {
        NX_ASSERT(false); /* Not supported for now. */
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
        NX_ASSERT(serializer);
        m_serializerByKey.insert(key, serializer);
    }

private:
    QHash<QString, QnJsonSerializer *> m_serializerByKey;
};

Q_GLOBAL_STATIC(QnResourceDataJsonSerializer, qn_resourceDataJsonSerializer_instance)

bool QnResourceData::value(const QString &key, int type, void *value, const CopyFunction &copyFunction) const {
    auto pos = m_dataByKey.constFind(key);
    if(pos == m_dataByKey.constEnd())
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
    NX_ASSERT(serializer);

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
