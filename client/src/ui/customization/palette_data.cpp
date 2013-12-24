#include "palette_data.h"

#include <cassert>

#include <boost/array.hpp>

#include <utils/common/json.h>


// -------------------------------------------------------------------------- //
// QnPaletteData
// -------------------------------------------------------------------------- //
typedef boost::array<QColor, QPalette::NColorRoles> QnColorGroup;
typedef boost::array<QnColorGroup, QPalette::NColorGroups> QnPaletteColors;

class QnPaletteDataPrivate: public QSharedData {
public:
    QnPaletteColors colors;

    template<class Functor>
    void forEachColor(const Functor &functor) const {
        for(QPalette::ColorGroup group = static_cast<QPalette::ColorGroup>(0); group < QPalette::NColorGroups; group = static_cast<QPalette::ColorGroup>(group + 1))
            for(QPalette::ColorRole role = static_cast<QPalette::ColorRole>(0); role < QPalette::NColorRoles; role = static_cast<QPalette::ColorRole>(role + 1))
                functor(group, role);
    }
};

QnPaletteData::QnPaletteData(const QPalette &palette): d(new QnPaletteDataPrivate) {
    d->forEachColor([&](QPalette::ColorGroup group, QPalette::ColorRole role) {
        setColor(group, role, palette.color(group, role));
    });
}

QnPaletteData::QnPaletteData(): d(new QnPaletteDataPrivate) {
    return;
}

QnPaletteData::~QnPaletteData() {
    return;
}

void QnPaletteData::apply(QPalette *palette) {
    assert(palette);

    d->forEachColor([&](QPalette::ColorGroup group, QPalette::ColorRole role) {
        const QColor &color = d->colors[group][role];
        if(color.isValid())
            palette->setColor(group, role, color);
    });
}

const QColor &QnPaletteData::color(QPalette::ColorGroup group, QPalette::ColorRole role) const {
    return d->colors[group][role];
}

void QnPaletteData::setColor(QPalette::ColorGroup group, QPalette::ColorRole role, const QColor &color) {
    d.detach();
    d->colors[group][role] = color;
}


// -------------------------------------------------------------------------- //
// Serialization
// -------------------------------------------------------------------------- //
#define FOR_EACH_COLOR_ROLE(MACRO)                                              \
    MACRO(QPalette::WindowText,     lit("windowText"))                          \
    MACRO(QPalette::Button,         lit("button"))                              \
    MACRO(QPalette::Light,          lit("light"))                               \
    MACRO(QPalette::Midlight,       lit("midlight"))                            \
    MACRO(QPalette::Dark,           lit("dark"))                                \
    MACRO(QPalette::Mid,            lit("mid"))                                 \
    MACRO(QPalette::Text,           lit("text"))                                \
    MACRO(QPalette::BrightText,     lit("brightText"))                          \
    MACRO(QPalette::ButtonText,     lit("buttonText"))                          \
    MACRO(QPalette::Base,           lit("base"))                                \
    MACRO(QPalette::Window,         lit("window"))                              \
    MACRO(QPalette::Shadow,         lit("shadow"))                              \
    MACRO(QPalette::Highlight,      lit("highlight"))                           \
    MACRO(QPalette::HighlightedText, lit("highlightedText"))                    \
    MACRO(QPalette::Link,           lit("link"))                                \
    MACRO(QPalette::LinkVisited,    lit("linkVisited"))                         \
    MACRO(QPalette::AlternateBase,  lit("alternateBase"))                       \
    MACRO(QPalette::ToolTipBase,    lit("toolTipBase"))                         \
    MACRO(QPalette::ToolTipText,    lit("toolTipText"))

void serialize(const QnColorGroup &value, QJsonValue *target) {
    QJsonObject map;

#define STEP(ROLE, NAME)                                                        \
    {                                                                           \
        const QColor &color = value[ROLE];                                      \
        if(color.isValid())                                                     \
            QJson::serialize(color, NAME, &map);                                \
    }
    FOR_EACH_COLOR_ROLE(STEP)
#undef STEP

    *target = map;
}

bool deserialize(const QJsonValue &value, QnColorGroup *target) {
    QJsonObject map;
    if(!QJson::deserialize(value, &map))
        return false;

    QnColorGroup result;
    if(
#define STEP(ROLE, NAME)                                                        \
        !QJson::deserialize(map, NAME, &result[ROLE], true) ||
    FOR_EACH_COLOR_ROLE(STEP)
#undef STEP
        false
    ) {
        return false;
    }

    *target = result;
    return true;
}

void serialize(const QnPaletteColors &value, QJsonValue *target) {
    QJsonObject map;

    QJson::serialize(value[QPalette::Active],   lit("active"),      &map);
    QJson::serialize(value[QPalette::Disabled], lit("disabled"),    &map);
    QJson::serialize(value[QPalette::Inactive], lit("inactive"),    &map);

    *target = map;
}

bool deserialize(const QJsonValue &value, QnPaletteColors *target) {
    QJsonObject map;
    if(!QJson::deserialize(value, &map))
        return false;

    QnColorGroup defaultGroup;
    if(!QJson::deserialize(map, lit("default"), &defaultGroup, true))
        return false;

    QnPaletteColors result;
    for(int i = 0; i < QPalette::NColorGroups; i++)
        result[i] = defaultGroup;

    if(
        !QJson::deserialize(map, lit("active"),     &result[QPalette::Active],      true) ||
        !QJson::deserialize(map, lit("disabled"),   &result[QPalette::Disabled],    true) ||
        !QJson::deserialize(map, lit("inactive"),   &result[QPalette::Inactive],    true)
    ) {
        return false;
    }

    *target = result;
    return true;
}

void serialize(const QnPaletteData &value, QJsonValue *target) {
    serialize(value.d->colors, target);
}

bool deserialize(const QJsonValue &value, QnPaletteData *target) {
    target->d.detach();
    return deserialize(value, &target->d->colors);
}

