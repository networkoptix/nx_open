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
typedef QColor QnColorGroup[QPalette::NColorRoles];
typedef QnColorGroup QnPaletteColors[QPalette::NColorGroups + 1];

bool deserialize(const QJsonValue &value, QnColorGroup *target) {
    QJsonObject map;
    if(!QJson::deserialize(value, &map))
        return false;

    return
        QJson::deserialize(map, lit("windowText"),       &(*target)[QPalette::WindowText],       true) && 
        QJson::deserialize(map, lit("button"),           &(*target)[QPalette::Button],           true) && 
        QJson::deserialize(map, lit("light"),            &(*target)[QPalette::Light],            true) && 
        QJson::deserialize(map, lit("midlight"),         &(*target)[QPalette::Midlight],         true) && 
        QJson::deserialize(map, lit("dark"),             &(*target)[QPalette::Dark],             true) && 
        QJson::deserialize(map, lit("mid"),              &(*target)[QPalette::Mid],              true) && 
        QJson::deserialize(map, lit("text"),             &(*target)[QPalette::Text],             true) && 
        QJson::deserialize(map, lit("brightText"),       &(*target)[QPalette::BrightText],       true) && 
        QJson::deserialize(map, lit("buttonText"),       &(*target)[QPalette::ButtonText],       true) && 
        QJson::deserialize(map, lit("base"),             &(*target)[QPalette::Base],             true) && 
        QJson::deserialize(map, lit("window"),           &(*target)[QPalette::Window],           true) && 
        QJson::deserialize(map, lit("shadow"),           &(*target)[QPalette::Shadow],           true) && 
        QJson::deserialize(map, lit("highlight"),        &(*target)[QPalette::Highlight],        true) && 
        QJson::deserialize(map, lit("highlightedText"),  &(*target)[QPalette::HighlightedText],  true) && 
        QJson::deserialize(map, lit("link"),             &(*target)[QPalette::Link],             true) && 
        QJson::deserialize(map, lit("linkVisited"),      &(*target)[QPalette::LinkVisited],      true) && 
        QJson::deserialize(map, lit("alternateBase"),    &(*target)[QPalette::AlternateBase],    true) && 
        QJson::deserialize(map, lit("toolTipBase"),      &(*target)[QPalette::ToolTipBase],      true) && 
        QJson::deserialize(map, lit("toolTipText"),      &(*target)[QPalette::ToolTipText],      true);
}

bool deserialize(const QJsonValue &value, QnPaletteColors *target) {
    QJsonObject map;
    if(!QJson::deserialize(value, &map))
        return false;

    return 
        QJson::deserialize(map, lit("disabled"),         &(*target)[QPalette::Disabled],         true) && 
        QJson::deserialize(map, lit("active"),           &(*target)[QPalette::Active],           true) && 
        QJson::deserialize(map, lit("inactive"),         &(*target)[QPalette::Inactive],         true) && 
        QJson::deserialize(map, lit("default"),          &(*target)[QPalette::NColorGroups],     true);
}

bool deserialize(const QJsonValue &value, QPalette *target) {
    QnPaletteColors colors;
    if(!QJson::deserialize(value, &colors))
        return false;

    for(int role = 0; role < QPalette::NColorRoles; role++) {
        const QColor &color = colors[QPalette::NColorGroups][role];
        if(color.isValid())
            target->setColor(static_cast<QPalette::ColorRole>(role), color);
    }

    for(int group = 0; group < QPalette::NColorGroups; group++) {
        for(int role = 0; role < QPalette::NColorRoles; role++) {
            const QColor &color = colors[group][role];
            if(color.isValid())
                target->setColor(static_cast<QPalette::ColorGroup>(group), static_cast<QPalette::ColorRole>(role), color);
        }
    }

    return true;
}

void serialize(const QPalette &value, QJsonValue *target) {
    assert(false);
}

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
// QnCustomizer
// -------------------------------------------------------------------------- //
QnCustomizer::QnCustomizer(QObject *parent):
    QObject(parent),
    m_serializer(new QnObjectStarCustomizationSerializer())
{}

QnCustomizer::QnCustomizer(const QnCustomization &customization, QObject *parent):
    QObject(parent),
    m_serializer(new QnObjectStarCustomizationSerializer())
{
    setCustomization(customization);
}

QnCustomizer::~QnCustomizer() {
    return;
}

void QnCustomizer::setCustomization(const QnCustomization &customization) {
    m_customization = customization;
}

const QnCustomization &QnCustomizer::customization() const {
    return m_customization;
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
        customize(object, m_customization.data(), QLatin1String(metaObjects[i]->className()));
}

void QnCustomizer::customize(QObject *object, const QJsonObject &customization, const QString &key) {
    auto pos = customization.find(key);
    if(pos == customization.end())
        return;
    
    m_serializer->deserialize(*pos, &object);
}
