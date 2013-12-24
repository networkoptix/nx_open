#include "customizer.h"

#include <utils/common/warnings.h>
#include <utils/common/json.h>
#include <utils/common/json_serializer.h>
#include <utils/common/flat_map.h>
#include <utils/common/property_storage.h>

// TODO: #Elric error messages


// --------------------------------------------------------------------------- //
// QnPaletteCustomizationSerializer
// --------------------------------------------------------------------------- //


typedef QnDefaultJsonSerializer<QPalette> QnPaletteCustomizationSerializer;


// --------------------------------------------------------------------------- //
// QnObjectStarCustomizationSerializer
// --------------------------------------------------------------------------- //
class QnObjectStarCustomizationSerializer: public QnJsonSerializer {
public:
    QnObjectStarCustomizationSerializer(): 
        QnJsonSerializer(QMetaType::QObjectStar),
        m_paletteSerializer(new QnPaletteCustomizationSerializer)
    {
        /* QnJsonSerializer does locking, so we use local cache to avoid it. */
        foreach(QnJsonSerializer *serializer, QnJsonSerializer::allSerializers())
            m_serializerByType.insert(serializer->type(), serializer);

        m_serializerByType.insert(QMetaType::QObjectStar, this);
        m_serializerByType.insert(QMetaType::QPalette, m_paletteSerializer.data());
    }

protected:
    virtual void serializeInternal(const void *, QJsonValue *) const override {
        assert(false); /* We should never get here. */
    }

    virtual bool deserializeInternal(const QJsonValue &value, void *target) const override {
        QObject *object = *static_cast<QObject **>(target);
        if(!object)
            return false;

        QJsonObject map;
        if(!QJson::deserialize(value, &map))
            return false;

        // TODO: #Elric hack
        if(object->inherits("QnPropertyStorage")) {
            QnPropertyStorage *storage = static_cast<QnPropertyStorage *>(object);
            storage->updateFromJson(map);
            return true;
        }

        const QMetaObject *metaObject = object->metaObject();

        for(auto pos = map.begin(); pos != map.end(); pos++) {
            const QString &key = pos.key();
            const QJsonValue &jsonValue = *pos;

            int index = metaObject->indexOfProperty(key.toLatin1());
            if(index != -1) {
                QMetaProperty p = metaObject->property(index);
                if (!p.isWritable())
                    return false;

                QVariant targetValue = p.read(object);
                const QnJsonSerializer *serializer = m_serializerByType.value(targetValue.userType());
                if(!serializer)
                    return false;

                if(!serializer->deserialize(jsonValue, &targetValue))
                    return false;

                p.write(object, targetValue);
                continue;
            }

            // TODO: #Elric extend properly
            if(object->inherits("QApplication")) {
                if(key == lit("palette")) {
                    const QnJsonSerializer *serializer = m_serializerByType.value(QMetaType::QPalette);
                    if(!serializer)
                        return false;

                    QPalette palette;
                    if(!serializer->deserialize(jsonValue, &palette))
                        return false;

                    static_cast<QApplication *>(object)->setPalette(palette);
                    continue;
                }
            }

            QObject *child = object->findChild<QObject *>(key);
            if(child != NULL) {
                if(!this->deserialize(jsonValue, &child))
                    return false;
                continue;
            }

            return false;
        }

        return true;
    }

private:
    QScopedPointer<QnJsonSerializer> m_paletteSerializer;
    QnFlatMap<int, QnJsonSerializer *> m_serializerByType;
};


// -------------------------------------------------------------------------- //
// QnCustomizationData
// -------------------------------------------------------------------------- //
class QnCustomizationData {
public:
    QnCustomizationData(): type(QMetaType::UnknownType) {}
    QnCustomizationData(const QJsonValue &json): type(QMetaType::UnknownType), json(json) {}

    int type;
    QVariant value;
    QJsonValue json;
};

typedef QHash<QString, QnCustomizationData> QnCustomizationDataHash;
Q_DECLARE_METATYPE(QnCustomizationDataHash)

bool deserialize(const QJsonValue &value, QnCustomizationData *target) {
    *target = QnCustomizationData();
    target->json = value;
    return true;
}




// -------------------------------------------------------------------------- //
// QnCustomizer
// -------------------------------------------------------------------------- //
class QnCustomizerPrivate {
public:
    QnCustomizerPrivate();

    void customize(QObject *object, const char *className);
    void customize(QObject *object, QnCustomizationData *data, const char *className);

    int hashType;
    QnCustomization customization;
    QHash<QLatin1String, QnCustomizationData> dataByClassName;
    QList<QByteArray> classNames;
};

QnCustomizerPrivate::QnCustomizerPrivate() {
    hashType = qMetaTypeId<QnCustomizationDataHash>();
}

QnCustomizer::QnCustomizer(QObject *parent):
    QObject(parent),
    d(new QnCustomizerPrivate())
{}

QnCustomizer::QnCustomizer(const QnCustomization &customization, QObject *parent):
    QObject(parent),
    d(new QnCustomizerPrivate())
{
    setCustomization(customization);
}

QnCustomizer::~QnCustomizer() {
    return;
}

void QnCustomizer::setCustomization(const QnCustomization &customization) {
    d->customization = customization;

    const QJsonObject &object = customization.data();
    for(auto pos = object.begin(); pos != object.end(); pos++) {
        QByteArray className = pos.key().toLatin1();
        d->classNames.push_back(className);
        d->dataByClassName[QLatin1String(className)] = QnCustomizationData(*pos);
    }
}

const QnCustomization &QnCustomizer::customization() const {
    return d->customization;
}

void QnCustomizer::customize(QObject *object) {
    if(!object) {
        qnNullWarning(object);
        return;
    }

    QVarLengthArray<const QMetaObject *, 32> metaObjects;

    const QMetaObject *metaObject = object->metaObject();
    while(metaObject) {
        metaObjects.append(metaObject);
        metaObject = metaObject->superClass();
    }

    for(int i = metaObjects.size() - 1; i >= 0; i--)
        customize(object, d->dataByClassName, QLatin1String(metaObjects[i]->className()));
}

void QnCustomizerPrivate::customize(QObject *object, const char *className) {
    auto pos = dataByClassName.find(QLatin1String(className));
    if(pos == dataByClassName.end())
        return;

    customize(object, &*pos);
}

void QnCustomizerPrivate::customize(QObject *object, QnCustomizationData *data, const char *className) {
    if(data->type != hashType) {
        data->type = hashType;
        
        QnCustomizationDataHash hash;
        if(!QJson::deserialize(data->json, &hash))
            qnWarning("Could not deserialize customization data for class '%1'.", className);

    }
}

