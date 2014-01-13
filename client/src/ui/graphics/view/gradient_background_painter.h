#ifndef QN_GRADIENT_BACKROUND_PAINTER_H
#define QN_GRADIENT_BACKROUND_PAINTER_H

#include <QtCore/QObject>
#include <QtCore/QElapsedTimer>
#include <QtCore/QScopedPointer>

#include <utils/common/json.h>

#include <client/client_color_types.h>

#include <ui/workbench/workbench_context_aware.h>
#include <ui/customization/customized.h>

#include "graphics_view.h" /* For QnLayerPainter. */

class VariantAnimator;

class QnClientSettings;
class QnRadialGradientPainter;

class QnGradientBackgroundPainter: public Customized<QObject>, public QnLayerPainter, public QnWorkbenchContextAware {
    Q_OBJECT
    Q_PROPERTY(QColor currentColor READ currentColor WRITE setCurrentColor)
    Q_PROPERTY(QnBackgroundColors colors READ colors WRITE setColors)
    typedef Customized<QObject> base_type;

public:
    /**
     * \param cycleIntervalSecs         Background animation cycle, in seconds.
     * \param parent                    Context-aware parent.
     */
    QnGradientBackgroundPainter(qreal cycleIntervalSecs, QObject *parent = NULL);

    virtual ~QnGradientBackgroundPainter();

    virtual void drawLayer(QPainter *painter, const QRectF &rect) override;

    const QnBackgroundColors &colors();
    void setColors(const QnBackgroundColors &colors);

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
    QScopedPointer<QnRadialGradientPainter> m_gradientPainter;

    VariantAnimator *m_backgroundColorAnimator;
    QElapsedTimer m_timer;

    QnBackgroundColors m_colors;
    QColor m_currentColor;
    qreal m_cycleIntervalSecs;
};

#endif // QN_GRADIENT_BACKROUND_PAINTER_H
