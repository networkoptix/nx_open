#ifndef QN_GRADIENT_BACKROUND_PAINTER_H
#define QN_GRADIENT_BACKROUND_PAINTER_H

#include <QtCore/QElapsedTimer>
#include <QtCore/QScopedPointer>
#include <QtCore/QWeakPointer>

#include "graphics_view.h" /* For QnLayerPainter. */

class QnSettings;
class QnGlFunctions;

class QnGradientBackgroundPainter: public QnLayerPainter {
public:
    /**
     * \param cycleIntervalSecs         Background animation cycle, in seconds.
     */
    QnGradientBackgroundPainter(qreal cycleIntervalSecs);

    virtual ~QnGradientBackgroundPainter();

    virtual void drawLayer(QPainter *painter, const QRectF &rect) override;

protected:
    virtual void installedNotify() override;

    /** 
     * \returns                         Current animation position, a number in
     *                                  range [-1, 1].
     */
    qreal position();

private:
    QColor backgroundColor() const;

private:
    QScopedPointer<QnGlFunctions> m_gl;
    QWeakPointer<QnSettings> m_settings;
    QElapsedTimer m_timer;
    qreal m_cycleIntervalSecs;
};

#endif // QN_GRADIENT_BACKROUND_PAINTER_H
