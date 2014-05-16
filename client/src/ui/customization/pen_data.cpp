#include "pen_data.h"

#include <cassert>

#include <utils/common/model_functions.h>

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qt, PenStyle, static)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qt, PenCapStyle, static)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qt, PenJoinStyle, static)

QN_FUSION_DEFINE_FUNCTIONS_FOR_TYPES(
    (Qt::PenStyle)(Qt::PenCapStyle)(Qt::PenJoinStyle),
    (json_lexical)
)


namespace QnPenDataDetail {
    template<QnPenData::Field field>
    struct Check {
        bool operator()(const QnPenData &data) const {
            return data.fields() & field;
        }
    };
}

QN_FUSION_ADAPT_CLASS_GSNC(
    QnPenData,
    ((&QnPenData::brush,        &QnPenData::setBrush,       "brush",        QnPenDataDetail::Check<QnPenData::Brush>()))
    ((&QnPenData::width,        &QnPenData::setWidth,       "width",        QnPenDataDetail::Check<QnPenData::Width>()))
    ((&QnPenData::style,        &QnPenData::setStyle,       "style",        QnPenDataDetail::Check<QnPenData::Style>()))
    ((&QnPenData::capStyle,     &QnPenData::setCapStyle,    "capStyle",     QnPenDataDetail::Check<QnPenData::CapStyle>()))
    ((&QnPenData::joinStyle,    &QnPenData::setJoinStyle,   "joinStyle",    QnPenDataDetail::Check<QnPenData::JoinStyle>())),
    (optional, true)
)

namespace QnPenDataDetail {
    QN_FUSION_DEFINE_FUNCTIONS(QnPenData, (json), inline)
}

QnPenData::QnPenData(): 
    m_fields(0), 
    m_width(0.0), 
    m_style(Qt::NoPen), 
    m_capStyle(Qt::SquareCap), 
    m_joinStyle(Qt::BevelJoin) 
{}

QnPenData::QnPenData(const QPen &pen):
    m_fields(AllFields),
    m_brush(pen.brush()),
    m_width(pen.width()),
    m_style(pen.style()),
    m_capStyle(pen.capStyle()),
    m_joinStyle(pen.joinStyle())
{}

void QnPenData::applyTo(QPen *pen) const {
    assert(pen);

    if(m_fields & Brush)        pen->setBrush(m_brush);
    if(m_fields & Width)        pen->setWidthF(m_width);
    if(m_fields & Style)        pen->setStyle(m_style);
    if(m_fields & CapStyle)     pen->setCapStyle(m_capStyle);
    if(m_fields & JoinStyle)    pen->setJoinStyle(m_joinStyle);
}

void serialize(QnJsonContext *ctx, const QnPenData &value, QJsonValue *target) {
    QnPenDataDetail::serialize(ctx, value, target);
}

bool deserialize(QnJsonContext *ctx, const QJsonValue &value, QnPenData *target) {
    QJsonObject map;
    if(!QJson::deserialize(ctx, value, &map))
        return false;

    QColor color;
    bool hasColor = false;
    if(!QJson::deserialize(ctx, map, lit("color"), &color, true, &hasColor))
        return false;

    if(!QnPenDataDetail::deserialize(ctx, value, target))
        return false;

    if(!(target->fields() & QnPenData::Brush) && hasColor)
        target->setBrush(color);

    return true;
}

