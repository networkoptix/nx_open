#include "popup_shadow.h"

#include <functional>
#include <utils/common/delayed.h>
#include <utils/common/scoped_painter_rollback.h>

/*
 * QnPopupShadowPrivate
 */
class QnPopupShadowPrivate
{
public:
    QnPopupShadowPrivate(QnPopupShadow* q, QWidget* popup) :
        m_popup(popup),
        m_shadow(new QLabel(popup->parentWidget())),
        m_color(Qt::black),
        m_offset(10, 10),
        m_blurRadius(10),
        m_spread(0),
        m_popupHadNativeShadow(!popup->windowFlags().testFlag(Qt::NoDropShadowWindowHint)),
        q_ptr(q)
    {
        m_shadow->setWindowFlags(m_shadow->windowFlags() | Qt::ToolTip | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
        m_shadow->setAttribute(Qt::WA_TranslucentBackground);
        m_shadow->setAutoFillBackground(false);

        m_color.setAlphaF(0.75);

        updateGeometry();

        m_shadow->setHidden(popup->isHidden());

        /* Turn off popup's native shadow: */
        if (m_popupHadNativeShadow)
            m_popup->setWindowFlags(m_popup->windowFlags() | Qt::NoDropShadowWindowHint);
    }

    ~QnPopupShadowPrivate()
    {
        /* Delete shadow widget explicitly, it's not our child: */
        delete m_shadow;

        /* Restore popup's native shadow if needed: */
        if (m_popup && m_popupHadNativeShadow && m_popup->testAttribute(Qt::WA_WState_Created))
            m_popup->setWindowFlags(m_popup->windowFlags() & ~Qt::NoDropShadowWindowHint);

    }

    void updateGeometry()
    {
        if (m_shadow.isNull() || m_popup.isNull())
            return;

        if (!m_popup->isVisible())
            return; /* will be called again from show event filter. */

        int growth = m_blurRadius + m_spread;
        QRect baseRect = m_popup->geometry().translated(m_offset);
        QRect shadowRect = baseRect.adjusted(-growth, -growth, growth, growth);
        m_widgetOffset = m_offset - QPoint(growth, growth);

        m_shadow->setGeometry(shadowRect);

        updatePixmap();
    }

    void updatePixmap()
    {
        if (m_shadow.isNull())
            return;

        m_shadow->setPixmap(createShadowPixmap(
            m_shadow->size(),
            m_shadow->devicePixelRatio(),
            m_blurRadius,
            m_color));
    }

    static QPixmap createShadowPixmap(const QSize& size, int pixelRatio, int blurRadius, const QColor& color)
    {
        /* Allocate new pixmap: */
        QPixmap pixmap(size * pixelRatio);
        pixmap.setDevicePixelRatio(pixelRatio);
        pixmap.fill(Qt::transparent);

        /* Create painter drawing to the pixmap: */
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, false);

        QColor fadedColor = color;
        fadedColor.setAlpha(0);

        QRectF fullRect(QPointF(0, 0), size);

        /* Simple case of no blur: */
        if (blurRadius == 0)
        {
            painter.fillRect(fullRect, color);
            return pixmap;
        }

        /* Gradient radius: */
        qreal radius = blurRadius * 2.0;

        /* Central rectangle with constant color, can be degenerate (have negative width and/or height): */
        QRectF centralRect = fullRect.adjusted(radius, radius, -radius, -radius);

        /* Rectangle formed by centers of the corners, never degenerate (singularity will be fixed): */
        QRectF cornersRect = centralRect;

        /* Handle singularity: */
        int singularity = 0;
        enum
        {
            DegenerateWidth  = 0x1,
            DegenerateHeight = 0x2
        };

        if (cornersRect.width() < 0)
        {
            singularity |= DegenerateWidth;
            cornersRect.setLeft((fullRect.left() + fullRect.right()) / 2.0);
            cornersRect.setWidth(0.0);
        }

        if (centralRect.height() < 0)
        {
            singularity |= DegenerateHeight;
            cornersRect.setTop((fullRect.top() + fullRect.bottom()) / 2.0);
            cornersRect.setHeight(0.0);
        }

        /* Draw left gradient, solid area and right gradient (top, bottom and corner gradients excluded): */
        if (!(singularity & DegenerateHeight))
        {
            QLinearGradient horizontalGradient = QLinearGradient(QPointF(0.0, 0.0), QPointF(fullRect.width(), 0.0));
            if (singularity & DegenerateWidth)
            {
                QColor centralColor = color;
                centralColor.setAlphaF(centralColor.alphaF() * (1.0 + centralRect.width() / (2.0 * radius)));
                horizontalGradient.setColorAt(0.0, fadedColor);
                horizontalGradient.setColorAt(0.5, centralColor);
                horizontalGradient.setColorAt(1.0, fadedColor);
            }
            else
            {
                qreal relativeFadeX = radius / fullRect.width();
                horizontalGradient.setColorAt(0.0, fadedColor);
                horizontalGradient.setColorAt(1.0, fadedColor);
                horizontalGradient.setColorAt(relativeFadeX, color);
                horizontalGradient.setColorAt(1.0 - relativeFadeX, color);
            }
            painter.fillRect(QRectF(fullRect.left(), centralRect.top(), fullRect.width(), centralRect.height()), horizontalGradient);
        }

        /* Draw top gradient and bottom gradient (corner gradients excluded): */
        if (!(singularity & DegenerateWidth))
        {
            QLinearGradient verticalGradient = QLinearGradient(QPointF(0.0, 0.0), QPointF(0.0, fullRect.height()));
            if (singularity & DegenerateHeight)
            {
                QColor centralColor = color;
                centralColor.setAlphaF(centralColor.alphaF() * (1.0 + centralRect.height() / (2.0 * radius)));
                verticalGradient.setColorAt(0.0, fadedColor);
                verticalGradient.setColorAt(0.5, centralColor);
                verticalGradient.setColorAt(1.0, fadedColor);
                painter.fillRect(QRectF(centralRect.left(), fullRect.top(), centralRect.width(), fullRect.height()), verticalGradient);
            }
            else
            {
                qreal relativeFadeY = radius / fullRect.height();
                verticalGradient.setColorAt(0.0, fadedColor);
                verticalGradient.setColorAt(1.0, fadedColor);
                verticalGradient.setColorAt(relativeFadeY, color);
                verticalGradient.setColorAt(1.0 - relativeFadeY, color);
                painter.fillRect(QRectF(centralRect.left(), fullRect.top(), centralRect.width(), radius), verticalGradient);        // top
                painter.fillRect(QRectF(centralRect.left(), centralRect.bottom(), centralRect.width(), radius), verticalGradient);  // bottom
            }
        }

        /* Draw corner gradients: */
        QRadialGradient radial;
        radial.setRadius(radius);
        radial.setColorAt(0.0, color);
        radial.setColorAt(1.0, fadedColor);

        radial.setCenter(centralRect.topLeft());
        radial.setFocalPoint(centralRect.topLeft());
        painter.fillRect(QRectF(cornersRect.topLeft(), fullRect.topLeft()), radial);

        radial.setCenter(centralRect.topRight());
        radial.setFocalPoint(centralRect.topRight());
        painter.fillRect(QRectF(cornersRect.topRight(), fullRect.topRight()), radial);

        radial.setCenter(centralRect.bottomRight());
        radial.setFocalPoint(centralRect.bottomRight());
        painter.fillRect(QRectF(cornersRect.bottomRight(), fullRect.bottomRight()), radial);

        radial.setCenter(centralRect.bottomLeft());
        radial.setFocalPoint(centralRect.bottomLeft());
        painter.fillRect(QRectF(cornersRect.bottomLeft(), fullRect.bottomLeft()), radial);

        return pixmap;
    }

    QPointer<QWidget> m_popup;
    QPointer<QLabel> m_shadow;

    QColor m_color;
    QPoint m_offset;
    int m_blurRadius;
    int m_spread;

    QPoint m_widgetOffset;

    bool m_popupHadNativeShadow;

    QnPopupShadow* q_ptr;
    Q_DECLARE_PUBLIC(QnPopupShadow)
};

/*
 * QnPopupShadow
 */
QnPopupShadow::QnPopupShadow(QWidget* popup) :
    QObject(popup),
    d_ptr(new QnPopupShadowPrivate(this, popup))
{
    popup->installEventFilter(this);
}

QnPopupShadow::~QnPopupShadow()
{
}

const QColor& QnPopupShadow::color() const
{
    Q_D(const QnPopupShadow);
    return d->m_color;
}

void QnPopupShadow::setColor(const QColor& color)
{
    Q_D(QnPopupShadow);
    if (d->m_color == color)
        return;

    d->m_color = color;
    d->updatePixmap();
}

QPoint QnPopupShadow::offset() const
{
    Q_D(const QnPopupShadow);
    return d->m_offset;
}

void QnPopupShadow::setOffset(const QPoint& offset)
{
    Q_D(QnPopupShadow);
    if (d->m_offset == offset)
        return;

    d->m_offset = offset;
    d->updateGeometry();
}

void QnPopupShadow::setOffset(int x, int y)
{
    setOffset(QPoint(x, y));
}

int QnPopupShadow::blurRadius() const
{
    Q_D(const QnPopupShadow);
    return d->m_blurRadius;
}

void QnPopupShadow::setBlurRadius(int blurRadius)
{
    Q_D(QnPopupShadow);
    if (d->m_blurRadius == blurRadius)
        return;

    d->m_blurRadius = blurRadius;
    d->updateGeometry();
}

int QnPopupShadow::spread() const
{
    Q_D(const QnPopupShadow);
    return d->m_spread;
}

void QnPopupShadow::setSpread(int spread)
{
    Q_D(QnPopupShadow);
    if (d->m_spread == spread)
        return;

    d->m_spread = spread;
    d->updateGeometry();
}

bool QnPopupShadow::eventFilter(QObject* object, QEvent* event)
{
    Q_D(QnPopupShadow);
    if (object == d->m_popup && !d->m_shadow.isNull())
    {
        switch (event->type())
        {
            case QEvent::Show:
            {
                d->updateGeometry();
                d->m_shadow->showNormal();
                d->m_popup->activateWindow();
                break;
            }

            case QEvent::Hide:
            {
                QPointer<QLabel> shadow(d->m_shadow);
                executeDelayed([shadow]()
                {
                    if (shadow)
                        shadow->hide();
                });
                break;
            }

            case QEvent::Move:
                d->m_shadow->move(d->m_popup->pos() + d->m_widgetOffset);
                break;

            case QEvent::Resize:
                d->updateGeometry();
                break;
        }
    }

    return base_type::eventFilter(object, event);
}
