#ifndef QN_VIEWPORT_GEOMETRY_ACCESSOR_H
#define QN_VIEWPORT_GEOMETRY_ACCESSOR_H

#include "accessor.h"
#include <QMargins>
#include <QRectF>
#include <ui/common/margin_flags.h>

class QGraphicsView;

class ViewportGeometryAccessor: public AbstractAccessor {
public:
    ViewportGeometryAccessor();

    virtual QVariant get(const QObject *object) const override;

    virtual void set(QObject *object, const QVariant &value) const override;

    const QMargins &margins() const {
        return m_margins;
    }

    void setMargins(const QMargins &margins) {
        m_margins = margins;
    }

    Qn::MarginFlags marginFlags() const {
        return m_marginFlags;
    }

    void setMarginFlags(Qn::MarginFlags marginFlags) {
        m_marginFlags = marginFlags;
    }

    QRectF aspectRatioAdjusted(QGraphicsView *view, const QRectF &sceneRect) const;

private:
    QMargins m_margins;
    Qn::MarginFlags m_marginFlags;
};

#endif // QN_VIEWPORT_GEOMETRY_ACCESSOR_H
