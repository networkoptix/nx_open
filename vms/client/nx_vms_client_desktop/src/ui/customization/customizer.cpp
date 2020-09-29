#include "customizer.h"

#include <algorithm>
#include <unordered_map>

#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsObject>

#include <ui/style/skin.h>

#include <nx/fusion/serialization/json_functions.h>
#include <nx/utils/flat_map.h>
#include <utils/common/property_storage.h>
#include <utils/common/evaluator.h>

#include <nx/utils/log/log.h>
#include <nx/utils/string.h>

#include "palette_data.h"
#include "pen_data.h"

namespace {

QGraphicsObject* findGraphicsChild(const QGraphicsObject* object, const QString& name)
{
    for (QGraphicsItem* childItem: object->childItems())
    {
        if (QGraphicsObject* childObject = childItem->toGraphicsObject())
        {
            if (childObject->objectName() == name)
                return childObject;
        }
    }

    for (QGraphicsItem* childItem: object->childItems())
    {
        if (QGraphicsObject* childObject = childItem->toGraphicsObject())
        {
            if (QGraphicsObject* result = findGraphicsChild(childObject, name))
                return result;
        }
    }

    return nullptr;
}

static const QString kPaletteProperty = "palette";

} // namespace

//-------------------------------------------------------------------------------------------------
// QnCustomizationAccessor

class QnCustomizationAccessor
{
public:
    virtual ~QnCustomizationAccessor() {}
    virtual QVariant read(const QObject* object, const QString& name) const = 0;
    virtual bool write(QObject* object, const QString& name, const QVariant& value) const = 0;
    virtual QObject* child(const QObject* object, const QString& name) const = 0;
};
using CustomizationAccessorPtr = std::unique_ptr<QnCustomizationAccessor>;

class QnObjectCustomizationAccessor: public QnCustomizationAccessor
{
public:
    virtual QVariant read(const QObject* object, const QString& name) const override
    {
        return object->property(name.toLatin1());
    }

    virtual bool write(QObject* object, const QString& name, const QVariant& value) const override
    {
        return object->setProperty(name.toLatin1(), value);
    }

    virtual QObject* child(const QObject* object, const QString& name) const override
    {
        return object->findChild<QObject*>(name);
    }
};

class QnGraphicsObjectCustomizationAccessor: public QnObjectCustomizationAccessor
{
    using base_type = QnObjectCustomizationAccessor;

public:
    virtual QObject* child(const QObject* object, const QString& name) const override
    {
        if (QObject* result = base_type::child(object, name))
            return result;

        return findGraphicsChild(static_cast<const QGraphicsObject*>(object), name);
    }
};

class QnApplicationCustomizationAccessor: public QnObjectCustomizationAccessor
{
    using base_type = QnObjectCustomizationAccessor;

public:
    virtual QVariant read(const QObject* object, const QString& name) const override
    {
        QVariant result = base_type::read(object, name);
        if (result.isValid())
            return result;

        if (name == kPaletteProperty)
            return QVariant::fromValue(static_cast<const QApplication*>(object)->palette());

        return QVariant();
    }

    virtual bool write(QObject* object, const QString& name, const QVariant& value) const override
    {
        bool result = base_type::write(object, name, value);
        if (result)
            return result;

        if (name == kPaletteProperty && value.userType() == QMetaType::QPalette)
        {
            static_cast<QApplication*>(object)->setPalette(
                *static_cast<const QPalette*>(value.data()));
            return true;
        }

        return false;
    }
};

class QnStorageCustomizationAccessor: public QnCustomizationAccessor
{
public:
    virtual QVariant read(const QObject* object, const QString& name) const override
    {
        return static_cast<const QnPropertyStorage*>(object)->value(name);
    }

    virtual bool write(QObject* object, const QString& name, const QVariant& value) const override
    {
        return static_cast<QnPropertyStorage*>(object)->setValue(name, value);
    }

    virtual QObject* child(const QObject*, const QString&) const override { return nullptr; }
};

template<class Base>
class QnCustomizationAccessorWrapper: public Base
{
    typedef Base base_type;

public:
    QnCustomizationAccessorWrapper()
    {
        m_paletteDataType = qMetaTypeId<QnPaletteData>();
        m_penDataType = qMetaTypeId<QnPenData>();

        m_dummyPaletteData = QVariant::fromValue(QnPaletteData());
        m_dummyPenData = QVariant::fromValue(QnPenData());
    }

    virtual QVariant read(const QObject* object, const QString& name) const override
    {
        QVariant result = base_type::read(object, name);

        /* Note that return value is not used, so we don't copy the underlying data. */
        switch (result.userType())
        {
            case QVariant::Pen:
                return m_dummyPenData;
            case QVariant::Palette:
                return m_dummyPaletteData;
            default:
                return result;
        }
    }

    virtual bool write(QObject* object, const QString& name, const QVariant& value) const override
    {
        if (value.userType() == m_paletteDataType)
        {
            QVariant objectValue = base_type::read(object, name);
            if (objectValue.userType() == QVariant::Palette)
            {
                static_cast<const QnPaletteData*>(value.data())
                    ->applyTo(static_cast<QPalette*>(objectValue.data()));
                return base_type::write(object, name, objectValue);
            }
            else
            {
                return false;
            }
        }
        else if (value.userType() == m_penDataType)
        {
            QVariant objectValue = base_type::read(object, name);
            if (objectValue.userType() == QVariant::Pen)
            {
                static_cast<const QnPenData*>(value.data())
                    ->applyTo(static_cast<QPen*>(objectValue.data()));
                return base_type::write(object, name, objectValue);
            }
            else
            {
                return false;
            }
        }
        else
        {
            return base_type::write(object, name, value);
        }
    }

private:
    int m_paletteDataType;
    int m_penDataType;
    QVariant m_dummyPaletteData;
    QVariant m_dummyPenData;
};

// -------------------------------------------------------------------------------------------------
// QnCustomizationData

class QnCustomizationData
{
public:
    QnCustomizationData(): type(QMetaType::UnknownType) {}
    QnCustomizationData(const QJsonValue& json): type(QMetaType::UnknownType), json(json) {}

    int type;
    QVariant value;
    QJsonValue json;
};

typedef QHash<QString, QnCustomizationData> QnCustomizationDataHash;
Q_DECLARE_METATYPE(QnCustomizationDataHash)

bool deserialize(QnJsonContext*, const QJsonValue& value, QnCustomizationData* target)
{
    *target = QnCustomizationData();
    target->json = value;
    return true;
}

void serialize(QnJsonContext*, const QnCustomizationData&, QJsonValue*)
{
    NX_ASSERT(false); /* We'll never get here. */
}

// --------------------------------------------------------------------------- //
// QnCustomizationSerializer
// --------------------------------------------------------------------------- //
Q_DECLARE_METATYPE(QnSkin*)

QVariant eval_skin(const Qee::ParameterPack& args)
{
    args.requireSize(0, 0);

    return QVariant::fromValue(qnSkin);
}

class QnCustomizationSerializer: public QObject, public QnJsonSerializer, public Qee::Resolver
{
public:
    QnCustomizationSerializer(int type, QObject* parent = NULL):
        QObject(parent), QnJsonSerializer(type)
    {
        m_evaluator.registerFunctions(Qee::ColorFunctions | Qee::ColorNames);
        m_evaluator.registerFunction("skin", &eval_skin);
        m_evaluator.setResolver(this);
    }

    const QnCustomizationDataHash& globals() const { return m_globals; }

    void setGlobals(const QnCustomizationDataHash& globals) { m_globals = globals; }

    QVariant globalConstant(const QString& name) const { return resolveConstant(name); }

protected:
    virtual void serializeInternal(
        QnJsonContext* ctx, const void* value, QJsonValue* target) const override
    {
        QJson::serialize(ctx, *static_cast<const QColor*>(value), target);
    }

    virtual bool deserializeInternal(
        QnJsonContext* ctx, const QJsonValue& value, void* target) const override
    {
        // TODO: #Elric use the easy way only for color codes.
        // We want to be able to override standard color names!

        if (QJson::deserialize(value, static_cast<QColor*>(target)))
            return true; /* Try the easy way first. */

        QString source;
        if (!QJson::deserialize(ctx, value, &source))
            return false;

        QVariant result;
        try
        {
            result = m_evaluator.evaluate(Qee::Parser::parse(source));
        }
        catch (const QnException& exception)
        {
            NX_ASSERT(false, exception.what());
            return false;
        }

        if (static_cast<QMetaType::Type>(result.type()) != QMetaType::QColor)
            return false;

        *static_cast<QColor*>(target) = *static_cast<QColor*>(result.data());
        return true;
    }

    virtual QVariant resolveConstant(const QString& name) const override
    {
        auto pos = const_cast<QnCustomizationSerializer*>(this)->m_globals.find(name);
        if (pos == m_globals.end())
            return QVariant();

        if (pos->type != QMetaType::UnknownType)
            return pos->value;

        pos->type = QMetaType::QColor;

        QnJsonContext ctx;
        if (!deserialize(&ctx, pos->json, &pos->value))
        {
            NX_ASSERT(false, "Could not deserialize global constant '%1'.", name);
            pos->value = QColor();
        }

        return pos->value;
    }

private:
    Qee::Evaluator m_evaluator;
    QnCustomizationDataHash m_globals;
};

// -------------------------------------------------------------------------- //
// Palette extraction
// -------------------------------------------------------------------------- //
namespace {

static const QString kCorePattern = "_core";
static const QString kDarkerPattern = "_d%1";
static const QString kLighterPattern = "_l%1";
static const QString kContrastPattern = "_contrast";

QnColorList extractCoreBasedColors(
    const QString& group,
    const QnCustomizationSerializer& serializer)
{
    QnColorList colors;

    enum class ColorType
    {
        NonMandatory,
        Mandatory,
    };

    const auto extractColor =
        [&serializer, group](const QString& suffix, ColorType type, QColor& result) -> bool
        {
            const auto key = group + suffix;
            if (!serializer.globals().contains(key))
                return false;

            const auto color = serializer.globalConstant(key).value<QColor>();
            if (type == ColorType::Mandatory)
                NX_ASSERT(color.isValid(), "Cannot deserialize color");

            result = color;
            return true;
        };

    QColor resultColor;
    if (extractColor(kCorePattern, ColorType::NonMandatory, resultColor))
        colors.setCoreColor(resultColor);

    if (extractColor(kContrastPattern, ColorType::NonMandatory, resultColor))
        colors.setContrastColor(resultColor);

    // For Compatibility. TODO: #ynikitenkov remove color with this index from everywhere in future
    colors.append(colors.coreColor());

    for (int idx = 1;; ++idx)
    {
        if (extractColor(kDarkerPattern.arg(idx), ColorType::Mandatory, resultColor))
            colors.prepend(resultColor);
        else
            break;
    }

    for (int idx = 1;; ++idx)
    {
        if (extractColor(kLighterPattern.arg(idx), ColorType::Mandatory, resultColor))
            colors.append(resultColor);
        else
            break;
    }

    return colors;
}

QnColorList extractColors(const QString& group, const QnCustomizationSerializer& serializer)
{
    if (serializer.globals().contains(group + kCorePattern))
        return extractCoreBasedColors(group, serializer);

    QnColorList colors;

    auto colorLess =
        [](const QColor& c1, const QColor& c2)
        {
            return c1.convertTo(QColor::Hsl).lightness() < c2.convertTo(QColor::Hsl).lightness();
        };

    for (const QString& constant: serializer.globals().keys())
    {
        if (!constant.startsWith(group))
            continue;

        QColor color = serializer.globalConstant(constant).value<QColor>();
        if (!color.isValid())
            continue;

        colors.insert(std::lower_bound(colors.begin(), colors.end(), color, colorLess), color);
    }

    return colors;
}
} // namespace

// -------------------------------------------------------------------------- //
// QnCustomizerPrivate
// -------------------------------------------------------------------------- //
class QnCustomizerPrivate
{
public:
    QnCustomizerPrivate(QnCustomizer* owner, QnCustomization customization);
    virtual ~QnCustomizerPrivate();

    void customize(QObject* object);
    void customize(QObject* object, QnCustomizationAccessor* accessor, const char* className);
    void customize(QObject* object,
        QnCustomizationData* data,
        QnCustomizationAccessor* accessor,
        const char* className);
    void customize(QObject* object,
        const QString& key,
        QnCustomizationData* data,
        QnCustomizationAccessor* accessor,
        const char* className);

    QnCustomizationAccessor* accessor(QObject* object) const;

public:
    const QnCustomizer* q;
    const QnCustomization customization;
    const int customizationHashType = qMetaTypeId<QnCustomizationDataHash>();

    std::unordered_map<std::string, QnCustomizationData> dataByClassName;
    std::unordered_map<std::string, QnCustomizationData> dataByObjectName;
    std::unordered_map<std::string, CustomizationAccessorPtr> accessorByClassName;
    CustomizationAccessorPtr defaultAccessor;
    QScopedPointer<QnCustomizationSerializer> colorSerializer;
    QScopedPointer<QnJsonSerializer> customizationHashSerializer;
    QnFlatMap<int, QnJsonSerializer*> serializerByType;
    QnJsonContext serializationContext;
    QnGenericPalette genericPalette;
};

QnCustomizerPrivate::QnCustomizerPrivate(QnCustomizer* owner, QnCustomization customization):
    q(owner),
    customization(std::move(customization))
{
    defaultAccessor.reset(new QnCustomizationAccessorWrapper<QnObjectCustomizationAccessor>());
    colorSerializer.reset(new QnCustomizationSerializer(QMetaType::QColor));
    customizationHashSerializer.reset(new QnDefaultJsonSerializer<QnCustomizationDataHash>());

    accessorByClassName.emplace("QApplication",
        new QnCustomizationAccessorWrapper<QnApplicationCustomizationAccessor>());
    accessorByClassName.emplace("QnPropertyStorage",
        new QnCustomizationAccessorWrapper<QnStorageCustomizationAccessor>());
    accessorByClassName.emplace("QGraphicsObject",
        new QnCustomizationAccessorWrapper<QnGraphicsObjectCustomizationAccessor>());

    /* QnJsonSerializer does locking, so we use local cache to avoid it. */
    foreach (QnJsonSerializer* serializer, QnJsonSerializer::serializers())
        serializerByType.insert(serializer->type(), serializer);
    serializerByType.insert(QMetaType::QColor, colorSerializer.data());
    serializerByType.insert(customizationHashType, customizationHashSerializer.data());
    serializationContext.registerSerializer(colorSerializer.data());
}

QnCustomizerPrivate::~QnCustomizerPrivate()
{
}

QnCustomizationAccessor* QnCustomizerPrivate::accessor(QObject* object) const
{
    const QMetaObject* metaObject = object->metaObject();

    while (metaObject)
    {
        auto iter = accessorByClassName.find(metaObject->className());
        if (iter != accessorByClassName.cend())
            return iter->second.get();

        metaObject = metaObject->superClass();
    }

    return defaultAccessor.get();
}

void QnCustomizerPrivate::customize(QObject* object)
{
    QVarLengthArray<const char*, 32> classNames;

    const QMetaObject* metaObject = object->metaObject();
    while (metaObject)
    {
        classNames.append(metaObject->className());
        metaObject = metaObject->superClass();
    }

    QnCustomizationAccessor* accessor = this->accessor(object);
    for (int i = classNames.size() - 1; i >= 0; i--)
        customize(object, accessor, classNames[i]);

    QString objectName = object->objectName();
    if (!objectName.isEmpty())
    {
        auto pos = dataByObjectName.find(objectName.toStdString());
        if (pos != dataByObjectName.end())
            customize(object, &pos->second, accessor, classNames[0]);
    }
}

void QnCustomizerPrivate::customize(
    QObject* object, QnCustomizationAccessor* accessor, const char* className)
{
    auto pos = dataByClassName.find(className);
    if (pos == dataByClassName.end())
        return;

    customize(object, &pos->second, accessor, className);
}

void QnCustomizerPrivate::customize(QObject* object,
    QnCustomizationData* data,
    QnCustomizationAccessor* accessor,
    const char* className)
{
    if (data->type != customizationHashType)
    {
        data->type = customizationHashType;

        QnCustomizationDataHash hash;
        if (!QJson::deserialize(&serializationContext, data->json, &hash))
        {
            NX_ASSERT(false, "Could not deserialize customization data for class '%1'.", className);
        }

        data->value = QVariant::fromValue(hash);
    }

    QnCustomizationDataHash& hash = *static_cast<QnCustomizationDataHash*>(data->value.data());
    for (auto pos = hash.begin(); pos != hash.end(); pos++)
        customize(object, pos.key(), &*pos, accessor, className);
}

void QnCustomizerPrivate::customize(QObject* object,
    const QString& key,
    QnCustomizationData* data,
    QnCustomizationAccessor* accessor,
    const char* className)
{
    // Reserved for comments.
    if (key.startsWith('_'))
        return;

    // Separate style, controlled by object name.
    if (key.startsWith('#'))
    {
        if (QObject* child = accessor->child(object, key.mid(1)))
            customize(child, data, this->accessor(child), child->metaObject()->className());
        return;
    }

    QVariant value = accessor->read(object, key);
    if (!value.isValid())
    {
        NX_ASSERT(false,
            "Could not customize property '%1' of class '%2'. "
            "The property is not defined for this class.",
            key,
            className);
        return;
    }

    int type = value.userType();
    if (data->type != type)
    {
        if (data->type != QMetaType::UnknownType)
            NX_ASSERT(false,
                "Property '%1' of class '%2' has different types "
                "for different instances of this class.",
                key,
                className);

        data->type = type;

        QnJsonSerializer* serializer = serializerByType.value(type);
        if (!serializer)
        {
            NX_ASSERT(false,
                "Could not customize property '%1' of class '%2'. "
                "No serializer is registered for type '%3'.",
                key,
                className,
                QMetaType::typeName(type));
            return;
        }

        if (!serializer->deserialize(&serializationContext, data->json, &data->value))
        {
            NX_ASSERT(false,
                "Could not customize property '%1' of class '%2'. "
                "Could not deserialize customization data.",
                key,
                className);
            return;
        }
    }

    /* This can happen if an error has occurred during deserialization.
     * Note that normally this check would not be needed, but there is a bug
     * in QObject::setProperty --- it could crash for user-defined types if
     * supplied the wrong type inside the variant. */ // TODO: #Elric #QTBUG write bugreport.
    if (data->value.userType() == QMetaType::UnknownType)
        return;

    if (!accessor->write(object, key, data->value))
    {
        NX_ASSERT(false,
            "Could not customize property '%1' of class '%2'. Property writing has failed.",
            key,
            className);
    }
}

// -------------------------------------------------------------------------- //
// QnCustomizer
// -------------------------------------------------------------------------- //

QnCustomizer::QnCustomizer(const QnCustomization& customization, QObject* parent):
    QObject(parent),
    d(new QnCustomizerPrivate(this, customization))
{
    const QJsonObject& object = customization.data();
    for (auto pos = object.begin(); pos != object.end(); pos++)
    {
        QString name = pos.key();
        if (name.startsWith('#'))
        {
            d->dataByObjectName[name.mid(1).toStdString()] = QnCustomizationData(*pos);
        }
        else
        {
            d->dataByClassName[name.toStdString()] = QnCustomizationData(*pos);
        }
    }

    /* Load globals. */
    auto pos = d->dataByClassName.find("globals");
    if (pos != d->dataByClassName.cend())
    {
        QnCustomizationDataHash globals;
        if (!QJson::deserialize(&d->serializationContext, pos->second.json, &globals))
        {
            NX_ASSERT(false, "Could not deserialize global constants block.");
        }
        else
        {
            d->colorSerializer->setGlobals(globals);

            QStringList colorGroups{{"dark", "light", "blue", "green", "brand", "red", "yellow"}};
            for (const QString& colorGroup: colorGroups)
            {
                d->genericPalette.setColors(
                    colorGroup,
                    extractColors(colorGroup, *d->colorSerializer));
            }
        }
    }
}

QnCustomizer::~QnCustomizer()
{
}

void QnCustomizer::customize(QObject* object)
{
    if (NX_ASSERT(object))
        d->customize(object);
}

QnGenericPalette QnCustomizer::genericPalette() const
{
    return d->genericPalette;
}
