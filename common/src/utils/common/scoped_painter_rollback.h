#ifndef QN_SCOPED_PAINTER_ROLLBACK_H
#define QN_SCOPED_PAINTER_ROLLBACK_H

#include "scoped_value_rollback.h"
#include <QPainter>

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

    struct PainterAntialiasingAccessor {
        void operator()(QPainter *painter, bool antialiasing) const {
            painter->setRenderHint(QPainter::Antialiasing, antialiasing);
        }

        bool operator()(QPainter *painter) const {
            return painter->testRenderHint(QPainter::Antialiasing);
        }
    };

} // namespace detail

typedef QnGenericScopedPainterRollback<QPen,        detail::PainterPenAccessor>             QnScopedPainterPenRollback;
typedef QnGenericScopedPainterRollback<QBrush,      detail::PainterBrushAccessor>           QnScopedPainterBrushRollback;
typedef QnGenericScopedPainterRollback<QTransform,  detail::PainterTransformAccessor>       QnScopedPainterTransformRollback;
typedef QnGenericScopedPainterRollback<QFont,       detail::PainterFontAccessor>            QnScopedPainterFontRollback;
typedef QnGenericScopedPainterRollback<bool,        detail::PainterAntialiasingAccessor>    QnScopedPainterAntialiasingRollback;


#endif // QN_SCOPED_PAINTER_ROLLBACK_H
