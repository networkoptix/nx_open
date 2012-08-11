#ifndef QN_GRADIENT_BACKROUND_PAINTER_H
#define QN_GRADIENT_BACKROUND_PAINTER_H

#include <QtCore/QObject>
#include <QtCore/QElapsedTimer>
#include <QtCore/QScopedPointer>
#include <QtCore/QWeakPointer>

#include <ui/workbench/workbench_context_aware.h>

#include "graphics_view.h" /* For QnLayerPainter. */

class VariantAnimator;

class QnGlFunctions;
class QnSettings;
class QnRadialGradientPainter;

class QnGradientBackgroundPainter: public QObject, public QnLayerPainter, public QnWorkbenchContextAware {
    Q_OBJECT;
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor);

public:
    /**
     * \param cycleIntervalSecs         Background animation cycle, in seconds.
     * \param parent                    Context-aware parent.
     */
    QnGradientBackgroundPainter(qreal cycleIntervalSecs, QObject *parent = NULL);

    virtual ~QnGradientBackgroundPainter();

    virtual void drawLayer(QPainter *painter, const QRectF &rect) override;

protected:
    virtual void installedNotify() override;

    /** 
     * \returns                         Current animation position, a number in
     *                                  range [-1, 1].
     */
    qreal position();

    QColor backgroundColor() const;
    void setBackgroundColor(const QColor &backgroundColor);

protected slots:
    void updateBackgroundColor(bool animate = true);

private:
    QScopedPointer<QnGlFunctions> m_gl;
    QScopedPointer<QnRadialGradientPainter> m_gradientPainter;
    
    QWeakPointer<QnSettings> m_settings;
    VariantAnimator *m_backgroundColorAnimator;
    QElapsedTimer m_timer;

    QColor m_backgroundColor;
    qreal m_cycleIntervalSecs;
};

#endif // QN_GRADIENT_BACKROUND_PAINTER_H
