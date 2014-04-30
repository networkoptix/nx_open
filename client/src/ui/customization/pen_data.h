#ifndef QN_PEN_DATA_H
#define QN_PEN_DATA_H

#include <QtCore/QMetaType>
#include <QtGui/QPen>

#include <utils/common/model_functions_fwd.h>

class QnPenData {
public:
    enum Field {
        Brush       = 0x01,
        Width       = 0x02,
        Style       = 0x04,
        CapStyle    = 0x08,
        JoinStyle   = 0x10,
        AllFields   = 0xFF
    };
    Q_DECLARE_FLAGS(Fields, Field)

    QnPenData();
    QnPenData(const QPen &pen);

    void applyTo(QPen *pen) const;

    Fields fields() const { return m_fields; }
    void setFields(Fields fields) { m_fields = fields; }

    const QBrush &brush() const { return m_brush; }
    void setBrush(const QBrush &brush) { m_brush = brush; m_fields |= Brush; }

    qreal width() const { return m_width; }
    void setWidth(qreal width) { m_width = width; m_fields |= Width; }

    Qt::PenStyle style() const { return m_style; }
    void setStyle(Qt::PenStyle style) { m_style = style; m_fields |= Style; }

    Qt::PenCapStyle capStyle() const { return m_capStyle; }
    void setCapStyle(Qt::PenCapStyle capStyle) { m_capStyle = capStyle; m_fields |= CapStyle; }

    Qt::PenJoinStyle joinStyle() const { return m_joinStyle; }
    void setJoinStyle(Qt::PenJoinStyle joinStyle) { m_joinStyle = joinStyle; m_fields |= JoinStyle; }

    QN_FUSION_DECLARE_FUNCTIONS(QnPenData, (json), friend)

private:
    Fields m_fields;
    QBrush m_brush;
    qreal m_width;
    Qt::PenStyle m_style;
    Qt::PenCapStyle m_capStyle;
    Qt::PenJoinStyle m_joinStyle;
};

Q_DECLARE_METATYPE(QnPenData)
    
#endif // QN_PEN_DATA_H
