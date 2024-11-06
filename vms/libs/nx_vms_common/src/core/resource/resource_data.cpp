// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/


#include <cassert>
#include <functional>

#include <api/model/api_ioport_data.h>
#include <core/dataprovider/stream_mixer_data.h>
#include <core/ptz/ptz_constants.h>
#include <core/ptz/ptz_mapper.h>
#include <core/resource/camera_advanced_param.h>
#include <core/resource/resource_data_structures.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/vms/api/data/credentials.h>
#include <nx/vms/common/ptz/override.h>

#include "resource_data.h"
#include "resource_property_key.h"

using namespace nx::vms::common;

class QnResourceDataJsonSerializer: public QnJsonSerializer {
public:
    QnResourceDataJsonSerializer():
        QnJsonSerializer(QMetaType::fromType<QnResourceData>())
    {
        registerKey<QnPtzMapperPtr>(lit("ptzMapper"));
        registerEnumKey<Ptz::Traits>(lit("ptzTraits"));
        registerKey<QStringList>(lit("vistaFocusDevices"));
        registerKey<QnIOPortDataList>(ResourceDataKey::kIoSettings);
        registerKey<QList<nx::vms::api::Credentials>>(
            ResourceDataKey::kPossibleDefaultCredentials);
        registerKey<nx::vms::api::Credentials>(ResourceDataKey::kForcedDefaultCredentials);
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
        registerKey<ptz::Override>(ptz::Override::kPtzOverrideKey);
        registerKey<QVector<bool>>("hasDualStreaming");
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

            if (const auto& deserializer = m_deserializerByKey.value(pos.key());
                deserializer.deserialize)
            {
                if(!deserializer.deserialize(ctx, data.json, &data.value))
                    return false;

                data.type = deserializer.type;
            }
        }

        *target = result;
        return true;
    }

private:
    template<typename T>
    void registerKey(const QString& key)
    {
        static_assert(!QnSerialization::IsInstrumentedEnumOrFlags<T>::value);
        const auto serializer = QnJsonSerializer::serializer(qMetaTypeId<T>());
        NX_ASSERT(serializer);
        m_deserializerByKey.insert(key,
            Deserializer{
                qMetaTypeId<T>(),
                [serializer](QnJsonContext* ctx, const QJsonValue& value, QVariant* target)
                {
                    return serializer->deserialize(ctx, value, target);
                }
            });
    }

    template<typename T>
    void registerEnumKey(const QString& key)
    {
        static_assert(QnSerialization::IsInstrumentedEnumOrFlags<T>::value);
        m_deserializerByKey.insert(key,
           Deserializer{
               qMetaTypeId<T>(),
                [](QnJsonContext*, const QJsonValue& value, QVariant* target)
                {
                    if (!value.isString())
                        return false;

                    T deserialized;
                    if (!nx::reflect::fromString(value.toString().toStdString(), &deserialized))
                        return false;

                    *target = QVariant::fromValue(deserialized);
                    return true;
                }
            });
    }

private:
    struct Deserializer
    {
        int type = 0;
        std::function<bool(QnJsonContext*, const QJsonValue&, QVariant*)> deserialize;
    };
    QHash<QString, Deserializer> m_deserializerByKey;
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
        NX_ASSERT(false, "Resource data for key '%1' was requested with a non-standard type '%2'.", key, QMetaType::typeName(type));

    QnJsonSerializer *serializer = QnJsonSerializer::serializer(type);
    if (!NX_ASSERT(serializer, "type %1, name '%2'", type, QMetaType::typeName(type)))
        return false;

    QnJsonContext ctx;
    return serializer->deserialize(&ctx, data.json, value);
}

void QnResourceData::clearKeyTags()
{
    m_dataByKey.removeIf(
        [](const decltype(m_dataByKey)::iterator it)
        {
            const QString& key = it.key();
            return key.compare(u"keys", Qt::CaseInsensitive) == 0 || key.startsWith("_");
        });
}

QJsonObject QnResourceData::allValuesRaw() const
{
    QJsonObject res;
    for (auto it = m_dataByKey.begin(); it != m_dataByKey.end(); ++it)
        res.insert(it.key(), it.value().json);
    return res;
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
