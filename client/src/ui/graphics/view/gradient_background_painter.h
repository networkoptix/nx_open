#ifndef QN_GRADIENT_BACKROUND_PAINTER_H
#define QN_GRADIENT_BACKROUND_PAINTER_H

#include <QtCore/QObject>
#include <QtCore/QElapsedTimer>
#include <QtCore/QScopedPointer>
#include <QtCore/QWeakPointer>

#include <utils/common/json.h>

#include <ui/workbench/workbench_context_aware.h>
#include <ui/customization/customized.h>

#include "graphics_view.h" /* For QnLayerPainter. */

class VariantAnimator;

class QnGlFunctions;
class QnClientSettings;
class QnRadialGradientPainter;

struct QnGradientBackgroundPainterColors {
    QnGradientBackgroundPainterColors();

    QColor normal;
    QColor panic;
};

Q_DECLARE_METATYPE(QnGradientBackgroundPainterColors)

QN_DEFINE_STRUCT_SERIALIZATION_FUNCTIONS_OPTIONAL(QnGradientBackgroundPainterColors, (normal)(panic), inline)


class QnGradientBackgroundPainter: public Customized<QObject>, public QnLayerPainter, public QnWorkbenchContextAware {
    Q_OBJECT
    Q_PROPERTY(QColor currentColor READ currentColor WRITE setCurrentColor)
    Q_PROPERTY(QnGradientBackgroundPainterColors colors READ colors WRITE setColors)
    typedef Customized<QObject> base_type;

public:
    /**
     * \param cycleIntervalSecs         Background animation cycle, in seconds.
     * \param parent                    Context-aware parent.
     */
    QnGradientBackgroundPainter(qreal cycleIntervalSecs, QObject *parent = NULL);

    virtual ~QnGradientBackgroundPainter();

    virtual void drawLayer(QPainter *painter, const QRectF &rect) override;

    const QnGradientBackgroundPainterColors &colors();
    void setColors(const QnGradientBackgroundPainterColors &colors);

protected:
    virtual void installedNotify() override;

    /**
     * \returns                         Current animation position, a number in
     *                                  range [-1, 1].
     */
    qreal position();

    QColor currentColor() const;
    void setCurrentColor(const QColor &backgroundColor);

    VariantAnimator *backgroundColorAnimator();

protected slots:
    void updateBackgroundColor(bool animate = true);

private:
    QScopedPointer<QnGlFunctions> m_gl;
    QScopedPointer<QnRadialGradientPainter> m_gradientPainter;

    VariantAnimator *m_backgroundColorAnimator;
    QElapsedTimer m_timer;

    QnGradientBackgroundPainterColors m_colors;
    QColor m_currentColor;
    qreal m_cycleIntervalSecs;
};

#endif // QN_GRADIENT_BACKROUND_PAINTER_H
