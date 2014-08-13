#ifndef QN_SCOPED_PAINTER_ROLLBACK_H
#define QN_SCOPED_PAINTER_ROLLBACK_H

#include "scoped_value_rollback.h"
#include <QtGui/QPainter>

template<class T, class Accessor>
class QnGenericScopedPainterRollback: public QnGenericScopedValueRollback<T, QPainter, Accessor, Accessor> {
    typedef QnGenericScopedValueRollback<T, QPainter, Accessor, Accessor> base_type;

public:
    QnGenericScopedPainterRollback(QPainter *painter): base_type(painter, Accessor(), Accessor()) {}

    QnGenericScopedPainterRollback(QPainter *painter, const T &newValue): base_type(painter, Accessor(), Accessor(), newValue) {}
};

namespace detail {
    struct PainterPenAccessor {
        void operator()(QPainter *painter, const QPen &pen) const {
            painter->setPen(pen);
        }

        QPen operator()(QPainter *painter) const {
            return painter->pen();
        }
    };

    struct PainterBrushAccessor {
        void operator()(QPainter *painter, const QBrush &brush) const {
            painter->setBrush(brush);
        }

        QBrush operator()(QPainter *painter) const {
            return painter->brush();
        }
    };

    struct PainterTransformAccessor {
        void operator()(QPainter *painter, const QTransform &transform) const {
            painter->setTransform(transform);
        }

        QTransform operator()(QPainter *painter) const {
            return painter->transform();
        }
    };

    struct PainterFontAccessor {
        void operator()(QPainter *painter, const QFont &font) const {
            painter->setFont(font);
        }

        QFont operator()(QPainter *painter) const {
            return painter->font();
        }
    };

    struct PainterOpacityAccessor {
        void operator()(QPainter *painter, qreal opacity) const {
            painter->setOpacity(opacity);
        }

        qreal operator()(QPainter *painter) const {
            return painter->opacity();
        }
    };

    struct PainterClipRegionAccessor {
        void operator()(QPainter *painter, const QRegion &clipRegion) const {
            if (clipRegion.isEmpty())
                painter->setClipping(false);
            else
                painter->setClipRegion(clipRegion);
        }

        QRegion operator()(QPainter *painter) const {
            return painter->clipRegion();
        }
    };

    template<QPainter::RenderHint renderHint>
    struct PainterRenderHintAccessor {
        void operator()(QPainter *painter, bool antialiasing) const {
            painter->setRenderHint(renderHint, antialiasing);
        }

        bool operator()(QPainter *painter) const {
            return painter->testRenderHint(renderHint);
        }
    };

} // namespace detail

typedef QnGenericScopedPainterRollback<QPen,        detail::PainterPenAccessor>                                             QnScopedPainterPenRollback;
typedef QnGenericScopedPainterRollback<QBrush,      detail::PainterBrushAccessor>                                           QnScopedPainterBrushRollback;
typedef QnGenericScopedPainterRollback<QTransform,  detail::PainterTransformAccessor>                                       QnScopedPainterTransformRollback;
typedef QnGenericScopedPainterRollback<QFont,       detail::PainterFontAccessor>                                            QnScopedPainterFontRollback;
typedef QnGenericScopedPainterRollback<qreal,       detail::PainterOpacityAccessor>                                         QnScopedPainterOpacityRollback;
typedef QnGenericScopedPainterRollback<QRegion,     detail::PainterClipRegionAccessor>                                      QnScopedPainterClipRegionRollback;
typedef QnGenericScopedPainterRollback<bool,        detail::PainterRenderHintAccessor<QPainter::Antialiasing> >             QnScopedPainterAntialiasingRollback;
typedef QnGenericScopedPainterRollback<bool,        detail::PainterRenderHintAccessor<QPainter::NonCosmeticDefaultPen> >    QnScopedPainterNonCosmeticDefaultPenRollback;
typedef QnGenericScopedPainterRollback<bool,        detail::PainterRenderHintAccessor<QPainter::SmoothPixmapTransform> >    QnScopedPainterSmoothPixmapTransformRollback;

#define QN_SCOPED_PAINTER_PEN_ROLLBACK(PAINTER, ... /* PEN */)                  \
    QN_GENERIC_SCOPED_ROLLBACK((QnGenericScopedPainterRollback<QPen, detail::PainterPenAccessor>), PAINTER, ##__VA_ARGS__)

#define QN_SCOPED_PAINTER_BRUSH_ROLLBACK(PAINTER, ... /* BRUSH */)              \
    QN_GENERIC_SCOPED_ROLLBACK((QnGenericScopedPainterRollback<QBrush, detail::PainterBrushAccessor>), PAINTER, ##__VA_ARGS__)

#define QN_SCOPED_PAINTER_TRANSFORM_ROLLBACK(PAINTER, ... /* TRANSFORM */)      \
    QN_GENERIC_SCOPED_ROLLBACK((QnGenericScopedPainterRollback<QTransform, detail::PainterTransformAccessor>), PAINTER, ##__VA_ARGS__)

// TODO: #Elric complete the list and remove typedefs

#endif // QN_SCOPED_PAINTER_ROLLBACK_H
