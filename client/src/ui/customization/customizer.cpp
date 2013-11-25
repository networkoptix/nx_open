#include "customizer.h"

#include <utils/common/warnings.h>
#include <utils/common/json.h>
#include <utils/common/json_serializer.h>
#include <utils/common/flat_map.h>

// TODO: #Elric error messages


// --------------------------------------------------------------------------- //
// QnPaletteCustomizationSerializer
// --------------------------------------------------------------------------- //
typedef QColor QnColorGroup[QPalette::NColorRoles];
typedef QnColorGroup QnPaletteColors[QPalette::NColorGroups + 1];

bool deserialize(const QVariant &value, QnColorGroup *target) {
    QVariantMap map;
    if(!QJson::deserialize(value, &map))
        return false;

    return
        QJson::deserialize(map, "windowText",       &(*target)[QPalette::WindowText],       true) && 
        QJson::deserialize(map, "button",           &(*target)[QPalette::Button],           true) && 
        QJson::deserialize(map, "light",            &(*target)[QPalette::Light],            true) && 
        QJson::deserialize(map, "midlight",         &(*target)[QPalette::Midlight],         true) && 
        QJson::deserialize(map, "dark",             &(*target)[QPalette::Dark],             true) && 
        QJson::deserialize(map, "mid",              &(*target)[QPalette::Mid],              true) && 
        QJson::deserialize(map, "text",             &(*target)[QPalette::Text],             true) && 
        QJson::deserialize(map, "brightText",       &(*target)[QPalette::BrightText],       true) && 
        QJson::deserialize(map, "buttonText",       &(*target)[QPalette::ButtonText],       true) && 
        QJson::deserialize(map, "base",             &(*target)[QPalette::Base],             true) && 
        QJson::deserialize(map, "window",           &(*target)[QPalette::Window],           true) && 
        QJson::deserialize(map, "shadow",           &(*target)[QPalette::Shadow],           true) && 
        QJson::deserialize(map, "highlight",        &(*target)[QPalette::Highlight],        true) && 
        QJson::deserialize(map, "highlightedText",  &(*target)[QPalette::HighlightedText],  true) && 
        QJson::deserialize(map, "link",             &(*target)[QPalette::Link],             true) && 
        QJson::deserialize(map, "linkVisited",      &(*target)[QPalette::LinkVisited],      true) && 
        QJson::deserialize(map, "alternateBase",    &(*target)[QPalette::AlternateBase],    true) && 
        QJson::deserialize(map, "toolTipBase",      &(*target)[QPalette::ToolTipBase],      true) && 
        QJson::deserialize(map, "toolTipText",      &(*target)[QPalette::ToolTipText],      true);
}

bool deserialize(const QVariant &value, QnPaletteColors *target) {
    QVariantMap map;
    if(!QJson::deserialize(value, &map))
        return false;

    return 
        QJson::deserialize(map, "disabled",         &(*target)[QPalette::Disabled],         true) && 
        QJson::deserialize(map, "active",           &(*target)[QPalette::Active],           true) && 
        QJson::deserialize(map, "inactive",         &(*target)[QPalette::Inactive],         true) && 
        QJson::deserialize(map, "default",          &(*target)[QPalette::NColorGroups],     true);
}

bool deserialize(const QVariant &value, QPalette *target) {
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

void serialize(const QPalette &value, QVariant *target) {
    assert(false);
}

typedef QnAdlJsonSerializer<QPalette> QnPaletteCustomizationSerializer;


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
    virtual void serializeInternal(const void *, QVariant *) const override {
        assert(false); /* We should never get here. */
    }

    virtual bool deserializeInternal(const QVariant &value, void *target) const override {
        QObject *object = *static_cast<QObject **>(target);
        if(!object)
            return false;

        const QMetaObject *metaObject = object->metaObject();

        QVariantMap map;
        if(!QJson::deserialize(value, &map))
            return false;

        for(QVariantMap::const_iterator pos = map.begin(); pos != map.end(); pos++) {
            const QString &key = pos.key();
            const QVariant &jsonValue = *pos;

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

QnCustomizer::~QnCustomizer() {
    return;
}

void QnCustomizer::setCustomization(const QString &customizationFileName) {
    QFile file(customizationFileName);
    if(!file.open(QFile::ReadOnly))
        return;

    QVariantMap customization;
    if(!QJson::deserialize(file.readAll(), &customization))
        return;

    setCustomization(customization);
}

void QnCustomizer::setCustomization(const QVariantMap &customization) {
    m_customization = customization;
}

const QVariantMap &QnCustomizer::customization() const {
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
        customize(object, QLatin1String(metaObjects[i]->className()));
}

void QnCustomizer::customize(QObject *object, const QString &key) {
    QVariantMap::const_iterator pos = m_customization.find(key);
    if(pos == m_customization.end())
        return;
    
    m_serializer->deserialize(*pos, &object);
}
