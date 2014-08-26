#ifndef QN_GRADIENT_BACKROUND_PAINTER_H
#define QN_GRADIENT_BACKROUND_PAINTER_H

#include <QtCore/QObject>
#include <QtCore/QElapsedTimer>
#include <QtCore/QScopedPointer>

#include <utils/serialization/json.h>

#include <client/client_color_types.h>

#include <ui/workbench/workbench_context_aware.h>
#include <ui/customization/customized.h>

#include "graphics_view.h" /* For QnLayerPainter. */

class VariantAnimator;

class QnClientSettings;
class QnRadialGradientPainter;

// TODO: #Elric move out?
class QnRainbow: public QObject {
    Q_OBJECT
public:
    QnRainbow(QObject *parent = NULL): 
        QObject(parent) 
    {
        m_currentIndex = 0;

        m_colors << 
            QColor(0xFFFF0000) <<
            QColor(0xFFFF7F00) <<
            QColor(0xFFFFFF00) <<
            QColor(0xFF00FF00) <<
            QColor(0xFF0000FF) <<
            QColor(0xFF4B0082) <<
            QColor(0xFF8B00FF);
    }

    const QColor &currentColor() const {
        return m_colors[m_currentIndex];
    }

    void advance() {
        m_currentIndex = (m_currentIndex + 1) % m_colors.size();
        emit currentColorChanged();
    }

signals:
    void currentColorChanged();

private:
    QVector<QColor> m_colors;
    int m_currentIndex;
};


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
    QnGradientBackgroundPainter(qreal cycleIntervalSecs, QObject *parent = NULL, QnWorkbenchContext *context = NULL);

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
    void updateBackgroundColor(bool animate);
    void updateBackgroundColorAnimated() { updateBackgroundColor(true); }

private:
    QScopedPointer<QnRadialGradientPainter> m_gradientPainter;

    VariantAnimator *m_backgroundColorAnimator;
    QElapsedTimer m_timer;

    QnBackgroundColors m_colors;
    QColor m_currentColor;
    qreal m_cycleIntervalSecs;
    
    QnRainbow *m_rainbow;
};

#endif // QN_GRADIENT_BACKROUND_PAINTER_H
