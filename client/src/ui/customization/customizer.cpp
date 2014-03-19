#include "customizer.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsObject>

#include <utils/common/warnings.h>
#include <utils/common/json.h>
#include <utils/common/json_serializer.h>
#include <utils/common/flat_map.h>
#include <utils/common/property_storage.h>
#include <utils/common/evaluator.h>

#include "palette_data.h"
#include "pen_data.h"

namespace {
    QGraphicsObject *findGraphicsChild(const QGraphicsObject *object, const QString &name) {
        foreach(QGraphicsItem *childItem, object->childItems())
            if(QGraphicsObject *childObject = childItem->toGraphicsObject())
                if(childObject->objectName() == name)
                    return childObject;

        foreach(QGraphicsItem *childItem, object->childItems())
            if(QGraphicsObject *childObject = childItem->toGraphicsObject())
                if(QGraphicsObject *result = findGraphicsChild(childObject, name))
                    return result;
        
        return NULL;
    }

} // anonymous namespace


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

class QnGraphicsObjectPropertyAccessor: public QnObjectPropertyAccessor {
    typedef QnObjectPropertyAccessor base_type;
public:
    virtual QVariant read(const QObject *object, const QString &name) const override {
        QVariant result = base_type::read(object, name);
        if(result.isValid())
            return result;

        if(QGraphicsObject *child = findGraphicsChild(static_cast<const QGraphicsObject *>(object), name))
            return QVariant::fromValue<QObject *>(child);

        return QVariant();
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

template<class Base>
class QnPropertyAccessorWrapper: public Base {
    typedef Base base_type;
public:
    QnPropertyAccessorWrapper() {
        m_paletteDataType = qMetaTypeId<QnPaletteData>();
        m_penDataType = qMetaTypeId<QnPenData>();

        m_dummyPaletteData = QVariant::fromValue(QnPaletteData());
        m_dummyPenData = QVariant::fromValue(QnPenData());
    }

    virtual QVariant read(const QObject *object, const QString &name) const {
        QVariant result = base_type::read(object, name);

        /* Note that return value is not used, so we don't copy the underlying data. */
        switch(result.userType()) {
        case QVariant::Pen:
            return m_dummyPenData;
        case QVariant::Palette:
            return m_dummyPaletteData;
        default:
            return result;
        }
    }

    virtual bool write(QObject *object, const QString &name, const QVariant &value) const {
        if(value.userType() == m_paletteDataType) {
            QVariant objectValue = base_type::read(object, name);
            if(objectValue.userType() == QVariant::Palette) {
                static_cast<const QnPaletteData *>(value.data())->applyTo(static_cast<QPalette *>(objectValue.data()));
                return base_type::write(object, name, objectValue);
            } else {
                return false;
            }
        } else if(value.userType() == m_penDataType) {
            QVariant objectValue = base_type::read(object, name);
            if(objectValue.userType() == QVariant::Pen) {
                static_cast<const QnPenData *>(value.data())->applyTo(static_cast<QPen *>(objectValue.data()));
                return base_type::write(object, name, objectValue);
            } else {
                return false;
            }
        } else {
            return base_type::write(object, name, value);
        }
    }

private:
    int m_paletteDataType;
    int m_penDataType;
    QVariant m_dummyPaletteData;
    QVariant m_dummyPenData;
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

bool deserialize(QnJsonContext *, const QJsonValue &value, QnCustomizationData *target) {
    *target = QnCustomizationData();
    target->json = value;
    return true;
}

void serialize(QnJsonContext *, const QnCustomizationData &, QJsonValue *) {
    assert(false); /* We'll never get here. */ 
}


// --------------------------------------------------------------------------- //
// QnCustomizationColorSerializer
// --------------------------------------------------------------------------- //
class QnCustomizationColorSerializer: public QObject, public QnJsonSerializer, public Qee::Resolver {
public:
    QnCustomizationColorSerializer(QObject *parent = NULL): 
        QObject(parent),
        QnJsonSerializer(QMetaType::QColor)
    {
        m_evaluator.registerFunctions(Qee::ColorFunctions);
        m_evaluator.setResolver(this);
    }

    const QnCustomizationDataHash &globals() const {
        return m_globals;
    }

    void setGlobals(const QnCustomizationDataHash &globals) {
        m_globals = globals;
    }

protected:
    virtual void serializeInternal(QnJsonContext *ctx, const void *value, QJsonValue *target) const override {
        QJson::serialize(ctx, *static_cast<const QColor *>(value), target);
    }

    virtual bool deserializeInternal(QnJsonContext *ctx, const QJsonValue &value, void *target) const override {
        if(QJson::deserialize(value, static_cast<QColor *>(target)))
            return true; /* Try the easy way first. */

        QString source;
        if(!QJson::deserialize(ctx, value, &source))
            return false;

        QVariant result;
        try {
            result = m_evaluator.evaluate(Qee::Parser::parse(source));
        } catch(const QnException &exception) {
            qnWarning(exception.what());
            return false;
        }

        if(result.type() != QMetaType::QColor)
            return false;

        *static_cast<QColor *>(target) = *static_cast<QColor *>(result.data());
        return true;
    }

    virtual QVariant resolveConstant(const QString &name) const override {
        auto pos = const_cast<QnCustomizationColorSerializer *>(this)->m_globals.find(name);
        if(pos == m_globals.end())
            return QVariant();

        if(pos->type != QMetaType::UnknownType)
            return pos->value;

        pos->type = QMetaType::QColor;

        QnJsonContext ctx;
        if(!deserialize(&ctx, pos->json, &pos->value)) {
            qnWarning("Could not deserialize global constant '%1'.", name);
            pos->value = QColor();
        }

        return pos->value;
    }

private:
    Qee::Evaluator m_evaluator;
    QnCustomizationDataHash m_globals;
};


// -------------------------------------------------------------------------- //
// QnCustomizerPrivate
// -------------------------------------------------------------------------- //
class QnCustomizerPrivate {
public:
    QnCustomizerPrivate();
    virtual ~QnCustomizerPrivate();

    void customize(QObject *object);
    void customize(QObject *object, QnPropertyAccessor *accessor, const char *className);
    void customize(QObject *object, QnCustomizationData *data, QnPropertyAccessor *accessor, const char *className);
    void customize(QObject *object, const QString &key, QnCustomizationData *data, QnPropertyAccessor *accessor, const char *className);

    QnPropertyAccessor *accessor(QObject *object) const;

    int customizationHashType;
    
    QnCustomization customization;
    QHash<QLatin1String, QnCustomizationData> dataByClassName;
    QList<QByteArray> classNames;
    QHash<QLatin1String, QnPropertyAccessor *> accessorByClassName;
    QScopedPointer<QnPropertyAccessor> defaultAccessor;
    QScopedPointer<QnCustomizationColorSerializer> colorSerializer;
    QScopedPointer<QnJsonSerializer> customizationHashSerializer;
    QnFlatMap<int, QnJsonSerializer *> serializerByType;
    QnJsonContext serializationContext;
};

QnCustomizerPrivate::QnCustomizerPrivate() {
    customizationHashType = qMetaTypeId<QnCustomizationDataHash>();

    defaultAccessor.reset(new QnPropertyAccessorWrapper<QnObjectPropertyAccessor>());
    colorSerializer.reset(new QnCustomizationColorSerializer());
    customizationHashSerializer.reset(new QnDefaultJsonSerializer<QnCustomizationDataHash>());
    
    accessorByClassName.insert(QLatin1String("QApplication"), new QnPropertyAccessorWrapper<QnApplicationPropertyAccessor>());
    accessorByClassName.insert(QLatin1String("QnPropertyStorage"), new QnPropertyAccessorWrapper<QnStoragePropertyAccessor>());
    accessorByClassName.insert(QLatin1String("QGraphicsObject"), new QnPropertyAccessorWrapper<QnGraphicsObjectPropertyAccessor>());

    /* QnJsonSerializer does locking, so we use local cache to avoid it. */
    foreach(QnJsonSerializer *serializer, QnJsonSerializer::allSerializers())
        serializerByType.insert(serializer->type(), serializer);
    serializerByType.insert(QMetaType::QColor, colorSerializer.data());
    serializerByType.insert(customizationHashType, customizationHashSerializer.data());
    serializationContext.registerSerializer(colorSerializer.data());
}

QnCustomizerPrivate::~QnCustomizerPrivate() {
    qDeleteAll(accessorByClassName);
}

QnPropertyAccessor *QnCustomizerPrivate::accessor(QObject *object) const {
    const QMetaObject *metaObject = object->metaObject();
    
    QnPropertyAccessor *result = NULL;
    while(metaObject) {
        if(result = accessorByClassName.value(QLatin1String(metaObject->className())))
            return result;
        metaObject = metaObject->superClass();
    }
    
    return defaultAccessor.data();
}

void QnCustomizerPrivate::customize(QObject *object) {
    QVarLengthArray<const char *, 32> classNames;

    const QMetaObject *metaObject = object->metaObject();
    while(metaObject) {
        classNames.append(metaObject->className());
        metaObject = metaObject->superClass();
    }

    QnPropertyAccessor *accessor = this->accessor(object);
    for(int i = classNames.size() - 1; i >= 0; i--)
        customize(object, accessor, classNames[i]);
}

void QnCustomizerPrivate::customize(QObject *object, QnPropertyAccessor *accessor, const char *className) {
    auto pos = dataByClassName.find(QLatin1String(className));
    if(pos == dataByClassName.end())
        return;

    customize(object, &*pos, accessor, className);
}

void QnCustomizerPrivate::customize(QObject *object, QnCustomizationData *data, QnPropertyAccessor *accessor, const char *className) {
    if(data->type != customizationHashType) {
        data->type = customizationHashType;

        QnCustomizationDataHash hash;
        if(!QJson::deserialize(&serializationContext, data->json, &hash))
            qnWarning("Could not deserialize customization data for class '%1'.", className);

        data->value = QVariant::fromValue(hash);
    }

    QnCustomizationDataHash &hash = *static_cast<QnCustomizationDataHash *>(data->value.data());
    for(auto pos = hash.begin(); pos != hash.end(); pos++)
        customize(object, pos.key(), &*pos, accessor, className);
}

void QnCustomizerPrivate::customize(QObject *object, const QString &key, QnCustomizationData *data, QnPropertyAccessor *accessor, const char *className) {
    if(key.startsWith(L'_'))
        return; /* Reserved for comments. */

    QVariant value = accessor->read(object, key);
    if(!value.isValid()) {
        qnWarning("Could not customize property '%1' of class '%2'. The property is not defined for this class.", key, className);
        return;
    }

    int type = value.userType();
    bool isObject = false;

    if(type == QMetaType::QObjectStar) {
        type = customizationHashType;
        isObject = true;
    }

    if(data->type != type) {
        if(data->type != QMetaType::UnknownType)
            qnWarning("Property '%1' of class '%2' has different types for different instances of this class.", key, className);

        data->type = type;

        QnJsonSerializer *serializer = serializerByType.value(type);
        if(!serializer) {
            qnWarning("Could not customize property '%1' of class '%2'. No serializer is registered for type '%3'.", key, className, QMetaType::typeName(type));
            return;
        }

        if(!serializer->deserialize(&serializationContext, data->json, &data->value)) {
            qnWarning("Could not customize property '%1' of class '%2'. Could not deserialize customization data.", key, className);
            return;
        }
    }

    /* This can happen if an error has occurred during deserialization.
     * Note that normally this check would not be needed, but there is a bug
     * in QObject::setProperty --- it could crash for user-defined types if 
     * supplied the wrong type inside the variant. */ // TODO: #Elric #QTBUG write bugreport.
    if(data->value.userType() == QMetaType::UnknownType)
        return; 

    if(isObject) {
        QObject *subObject = value.value<QObject *>();
        customize(subObject, data, this->accessor(subObject), subObject->metaObject()->className());
    } else {
        if(!accessor->write(object, key, data->value))
            qnWarning("Could not customize property '%1' of class '%2'. Property writing has failed.", key, className);
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

    /* Load globals. */
    auto pos = d->dataByClassName.find(QLatin1String("globals"));
    if(pos != d->dataByClassName.end()) {
        QnCustomizationDataHash globals;
        if(!QJson::deserialize(&d->serializationContext, pos->json, &globals)) {
            qnWarning("Could not deserialize global constants block.");
        } else {
            d->colorSerializer->setGlobals(globals);
        }
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

