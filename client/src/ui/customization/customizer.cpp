#include "customizer.h"

#include <utils/common/warnings.h>
#include <utils/common/json.h>
#include <utils/common/json_serializer.h>
#include <utils/common/flat_map.h>
#include <utils/common/property_storage.h>
#include <utils/common/evaluator.h>

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
// QnCustomizationColorSerializer
// --------------------------------------------------------------------------- //
class QnCustomizationColorSerializer: public QObject, public QnJsonSerializer, public QnPropertyAccessor {
public:
    QnCustomizationColorSerializer(QObject *parent = NULL): 
        QObject(parent),
        QnJsonSerializer(QMetaType::QColor)
    {
        m_evaluator.registerFunctions(Qee::ColorFunctions);
    }

protected:
    virtual void serializeInternal(const void *value, QJsonValue *target) const override {
        QJson::serialize(*static_cast<const QColor *>(value), target);
    }

    virtual bool deserializeInternal(const QJsonValue &value, void *target) const override {
        if(QJson::deserialize(value, static_cast<QColor *>(target)))
            return true; /* Try the easy way first. */

        QString source;
        if(!QJson::deserialize(value, &source))
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

    virtual QVariant read(const QObject *object, const QString &name) const {
        QVariant result = static_cast<const QnCustomizationColorSerializer *>(object)->m_evaluator.constant(name);
        return result.isValid() ? result : QVariant(QMetaType::QColor, NULL);
    }

    virtual bool write(QObject *object, const QString &name, const QVariant &value) const {
        static_cast<QnCustomizationColorSerializer *>(object)->m_evaluator.registerConstant(name, value);
        return true;
    }

private:
    Qee::Evaluator m_evaluator;
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
    void customize(QObject *object, QnPropertyAccessor *accessor, const char *className);
    void customize(QObject *object, QnCustomizationData *data, QnPropertyAccessor *accessor, const char *className);
    void customize(QObject *object, const QString &key, QnCustomizationData *data, QnPropertyAccessor *accessor, const char *className);

    int customizationHashType;
    int paletteDataType;
    
    QnCustomization customization;
    QHash<QLatin1String, QnCustomizationData> dataByClassName;
    QList<QByteArray> classNames;
    QHash<QLatin1String, QnPropertyAccessor *> accessorByClassName;
    QScopedPointer<QnPropertyAccessor> defaultAccessor;
    QScopedPointer<QnCustomizationColorSerializer> colorSerializer;
    QnFlatMap<int, QnJsonSerializer *> serializerByType;
};

QnCustomizerPrivate::QnCustomizerPrivate() {
    customizationHashType = qMetaTypeId<QnCustomizationDataHash>();
    paletteDataType = qMetaTypeId<QnPaletteData>();

    defaultAccessor.reset(new QnObjectPropertyAccessor());
    colorSerializer.reset(new QnCustomizationColorSerializer());
    
    accessorByClassName.insert(QLatin1String("QApplication"), new QnApplicationPropertyAccessor());
    accessorByClassName.insert(QLatin1String("QnPropertyStorage"), new QnStoragePropertyAccessor());

    /* QnJsonSerializer does locking, so we use local cache to avoid it. */
    foreach(QnJsonSerializer *serializer, QnJsonSerializer::allSerializers())
        serializerByType.insert(serializer->type(), serializer);
    serializerByType.insert(QMetaType::QColor, colorSerializer.data());
}

QnCustomizerPrivate::~QnCustomizerPrivate() {
    qDeleteAll(accessorByClassName);
}

void QnCustomizerPrivate::customize(QObject *object) {
    QVarLengthArray<const char *, 32> classNames;

    const QMetaObject *metaObject = object->metaObject();
    while(metaObject) {
        classNames.append(metaObject->className());
        metaObject = metaObject->superClass();
    }

    /* Pick the most-specialized property accessor. */
    QnPropertyAccessor *accessor = NULL;
    for(int i = 0; i < classNames.size(); i++) {
        accessor = accessorByClassName.value(QLatin1String(classNames[i]));
        if(accessor)
            break;
    }
    if(!accessor)
        accessor = defaultAccessor.data();

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
        if(!QJson::deserialize(data->json, &hash))
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
    if(data->type != type) {
        if(data->type != QMetaType::UnknownType)
            qnWarning("Property '%1' of class '%2' has different types for different instances of this class.", key, className);

        data->type = type;

        int proxyType = type == QMetaType::QPalette ? paletteDataType : type;
        QnJsonSerializer *serializer = serializerByType.value(proxyType);
        if(!serializer) {
            qnWarning("Could not customize property '%1' of class '%2'. No serializer is registered for type '%3'.", key, className, QMetaType::typeName(proxyType));
            return;
        }

        if(!serializer->deserialize(data->json, &data->value)) {
            qnWarning("Could not customize property '%1' of class '%2'. Could not deserialize customization data.", key, className);
            return;
        }
    }

    if(type == QMetaType::QPalette) {
        static_cast<const QnPaletteData *>(data->value.data())->applyTo(static_cast<QPalette *>(value.data()));
    } else {
        value = data->value;
    }

    if(!accessor->write(object, key, value))
        qnWarning("Could not customize property '%1' of class '%2'. Property writing has failed.", key, className);
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
    d->customize(d->colorSerializer.data(), d->colorSerializer.data(), "globals");
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

