#include "customizer.h"

#include <utils/common/warnings.h>
#include <utils/common/json.h>
#include <utils/common/json_serializer.h>
#include <utils/common/flat_map.h>
#include <utils/common/property_storage.h>

#include "palette_data.h"


// --------------------------------------------------------------------------- //
// QnPropertyAccessor
// --------------------------------------------------------------------------- //
class QnPropertyAccessor {
public:
    virtual ~QnPropertyAccessor() {}
    virtual QVariant read(const QObject *object, const QString &name) const = 0;
    virtual bool write(QObject *object, const QString &name, const QVariant &value) const = 0;
};

class QnObjectPropertyAccessor: public QnPropertyAccessor {
public:
    virtual QVariant read(const QObject *object, const QString &name) const override {
        QVariant result = object->property(name.toLatin1());
        if(result.isValid())
            return result;

        if(QObject *child = object->findChild<QObject *>(name))
            return QVariant::fromValue(child);

        return QVariant();
    }

    virtual bool write(QObject *object, const QString &name, const QVariant &value) const override {
        return object->setProperty(name.toLatin1(), value);
    }
};

class QnApplicationPropertyAccessor: public QnObjectPropertyAccessor {
    typedef QnObjectPropertyAccessor base_type;
public:
    virtual QVariant read(const QObject *object, const QString &name) const override {
        QVariant result = base_type::read(object, name);
        if(result.isValid())
            return result;

        if(name == lit("palette"))
            return QVariant::fromValue(static_cast<const QApplication *>(object)->palette());
        
        return QVariant();
    }

    virtual bool write(QObject *object, const QString &name, const QVariant &value) const override {
        bool result = base_type::write(object, name, value);
        if(result)
            return result;

        if(name == lit("palette") && value.userType() == QMetaType::QPalette) {
            static_cast<QApplication *>(object)->setPalette(*static_cast<const QPalette *>(value.data()));
            return true;
        }

        return false;
    }
};

class QnStoragePropertyAccessor: public QnPropertyAccessor {
public:
    virtual QVariant read(const QObject *object, const QString &name) const override {
        return static_cast<const QnPropertyStorage *>(object)->value(name);
    }

    virtual bool write(QObject *object, const QString &name, const QVariant &value) const override {
        return static_cast<QnPropertyStorage *>(object)->setValue(name, value);
    }
};


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
// QnCustomizerPrivate
// -------------------------------------------------------------------------- //
class QnCustomizerPrivate {
public:
    QnCustomizerPrivate();
    virtual ~QnCustomizerPrivate();

    void customize(QObject *object);
    void customize(QObject *object, const char *className);
    void customize(QObject *object, QnCustomizationData *data, QnPropertyAccessor *accessor, const char *className);
    void customize(QObject *object, const QString &key, QnCustomizationData *data, QnPropertyAccessor *accessor, const char *className);

    int customizationHashType;
    int paletteDataType;
    
    QnCustomization customization;
    QHash<QLatin1String, QnCustomizationData> dataByClassName;
    QList<QByteArray> classNames;
    QHash<QLatin1String, QnPropertyAccessor *> accessorByClassName;
    QScopedPointer<QnPropertyAccessor> defaultAccessor;
    QnFlatMap<int, QnJsonSerializer *> serializerByType;
};

QnCustomizerPrivate::QnCustomizerPrivate() {
    customizationHashType = qMetaTypeId<QnCustomizationDataHash>();
    paletteDataType = qMetaTypeId<QnPaletteData>();

    defaultAccessor.reset(new QnObjectPropertyAccessor());
    
    accessorByClassName.insert(QLatin1String("QApplication"), new QnApplicationPropertyAccessor());
    accessorByClassName.insert(QLatin1String("QnPropertyStorage"), new QnStoragePropertyAccessor());

    /* QnJsonSerializer does locking, so we use local cache to avoid it. */
    foreach(QnJsonSerializer *serializer, QnJsonSerializer::allSerializers())
        serializerByType.insert(serializer->type(), serializer);
}

QnCustomizerPrivate::~QnCustomizerPrivate() {
    qDeleteAll(accessorByClassName);
}

void QnCustomizerPrivate::customize(QObject *object) {
    QVarLengthArray<const QMetaObject *, 32> metaObjects;

    const QMetaObject *metaObject = object->metaObject();
    while(metaObject) {
        metaObjects.append(metaObject);
        metaObject = metaObject->superClass();
    }

    for(int i = metaObjects.size() - 1; i >= 0; i--)
        customize(object, metaObjects[i]->className());
}

void QnCustomizerPrivate::customize(QObject *object, const char *className) {
    auto pos = dataByClassName.find(QLatin1String(className));
    if(pos == dataByClassName.end())
        return;

    customize(object, &*pos, accessorByClassName.value(QLatin1String(className), defaultAccessor.data()), className);
}

void QnCustomizerPrivate::customize(QObject *object, QnCustomizationData *data, QnPropertyAccessor *accessor, const char *className) {
    if(data->type != customizationHashType) {
        data->type = customizationHashType;

        QnCustomizationDataHash hash;
        if(!QJson::deserialize(data->json, &hash))
            qnWarning("Could not deserialize customization data for class '%1'.", className);

        data->value = QVariant::fromValue(hash);
    }

    QnCustomizationDataHash &hash = *static_cast<QnCustomizationDataHash *>(data->value.data());
    for(auto pos = hash.begin(); pos != hash.end(); pos++)
        customize(object, pos.key(), &*pos, accessor, className);
}

void QnCustomizerPrivate::customize(QObject *object, const QString &key, QnCustomizationData *data, QnPropertyAccessor *accessor, const char *className) {
    QVariant value = accessor->read(object, key);
    if(!value.isValid()) {
        qnWarning("Property '%1' is not defined for class '%2'.", key, className);
        return;
    }

    int type = value.type();
    if(data->type != type) {
        if(data->type != QMetaType::UnknownType)
            qnWarning("Property '%1' has different types for different instances of class '%2'.", key, className);

        data->type = type;

        int proxyType = type == QMetaType::QPalette ? paletteDataType : type;
        QnJsonSerializer *serializer = serializerByType.value(proxyType);
        if(!serializer) {
            qnWarning("No serializer is registered for type '%1'. Could not customize property '%2' of class '%3'.", QMetaType::typeName(proxyType), key, className);
            return;
        }

        if(serializer->deserialize(data->json, &data->value))
            qnWarning("Could not deserialize customization data for property '%1' of class '%2'.", key, className);
    }

}


// -------------------------------------------------------------------------- //
// QnCustomizer
// -------------------------------------------------------------------------- //
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

    d->customize(object);
}

