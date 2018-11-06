#include "popup_shadow.h"

#include <QtCore/QEvent>
#include <QtCore/QPointer>

#include <QtWidgets/QLabel>

#include <functional>
#include <utils/common/delayed.h>
#include <utils/common/scoped_painter_rollback.h>

namespace nx::vms::client::desktop {

//-------------------------------------------------------------------------------------------------
// PopupShadow::Private

struct PopupShadow::Private
{
public:
    Private(PopupShadow* q, QWidget* popup) :
        q(q),
        popup(popup),
        shadow(new QLabel(popup->parentWidget())),
        color(Qt::black),
        offset(10, 10),
        blurRadius(10),
        spread(0),
        popupHadNativeShadow(!popup->windowFlags().testFlag(Qt::NoDropShadowWindowHint))
    {
        QObject::connect(popup, &QObject::destroyed, q, [this]() { this->popup = nullptr; });

        const Qt::WindowFlags kExtraFlags(
            Qt::Popup |
            Qt::FramelessWindowHint |
            Qt::NoDropShadowWindowHint |
            Qt::WindowTransparentForInput |
            Qt::WindowDoesNotAcceptFocus |
            Qt::BypassGraphicsProxyWidget);

        shadow->setWindowFlags(shadow->windowFlags() | kExtraFlags);
        shadow->setAttribute(Qt::WA_TranslucentBackground);
        shadow->setAttribute(Qt::WA_ShowWithoutActivating);
        shadow->setAutoFillBackground(false);

        color.setAlphaF(0.75);

        updateGeometry();

        shadow->setHidden(popup->isHidden());

        /* Turn off popup's native shadow: */
        if (popupHadNativeShadow)
            popup->setWindowFlags(popup->windowFlags() | Qt::NoDropShadowWindowHint);
    }

    ~Private()
    {
        /* Delete shadow widget explicitly, it's not our child: */
        delete shadow;

        /* Restore popup's native shadow if needed: */
        if (popup && popupHadNativeShadow)
            popup->setWindowFlags(popup->windowFlags() & ~Qt::NoDropShadowWindowHint);
    }

    void updateGeometry()
    {
        if (!shadow || !popup)
            return;

        if (!popup->isVisible())
            return; /* will be called again from show event filter. */

        int growth = blurRadius + spread;
        QRect baseRect = popup->geometry().translated(offset);
        QRect shadowRect = baseRect.adjusted(-growth, -growth, growth, growth);
        widgetOffset = offset - QPoint(growth, growth);

        shadow->setGeometry(shadowRect);

        updatePixmap();
    }

    void updatePixmap()
    {
        if (shadow.isNull())
            return;

        shadow->setPixmap(createShadowPixmap(
            shadow->size(),
            shadow->devicePixelRatio(),
            blurRadius,
            color));
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

    PopupShadow* const q = nullptr;

    QWidget* popup = nullptr;
    QPointer<QLabel> shadow;

    QColor color = Qt::black;
    QPoint offset = QPoint(10, 10);
    int blurRadius = 10;
    int spread = 0;

    QPoint widgetOffset;

    bool popupHadNativeShadow = false;
};

//-------------------------------------------------------------------------------------------------
// PopupShadow

PopupShadow::PopupShadow(QWidget* popup):
    base_type(popup),
    d(new Private(this, popup))
{
    popup->installEventFilter(this);
}

PopupShadow::~PopupShadow()
{
}

QColor PopupShadow::color() const
{
    return d->color;
}

void PopupShadow::setColor(const QColor& color)
{
    if (d->color == color)
        return;

    d->color = color;
    d->updatePixmap();
}

QPoint PopupShadow::offset() const
{
    return d->offset;
}

void PopupShadow::setOffset(const QPoint& offset)
{
    if (d->offset == offset)
        return;

    d->offset = offset;
    d->updateGeometry();
}

void PopupShadow::setOffset(int x, int y)
{
    setOffset(QPoint(x, y));
}

int PopupShadow::blurRadius() const
{
    return d->blurRadius;
}

void PopupShadow::setBlurRadius(int blurRadius)
{
    if (d->blurRadius == blurRadius)
        return;

    d->blurRadius = blurRadius;
    d->updateGeometry();
}

int PopupShadow::spread() const
{
    return d->spread;
}

void PopupShadow::setSpread(int spread)
{
    if (d->spread == spread)
        return;

    d->spread = spread;
    d->updateGeometry();
}

bool PopupShadow::eventFilter(QObject* object, QEvent* event)
{
    if (object == d->popup && !d->shadow.isNull())
    {
        switch (event->type())
        {
            case QEvent::Show:
            {
                d->updateGeometry();
                d->shadow->setVisible(true);
                d->popup->raise();
                break;
            }

            case QEvent::Hide:
            {
                auto shadow = d->shadow;
                shadow->setGeometry(0, 0, 1, 1); // for immediate hide
                executeDelayedParented([shadow]() { shadow->hide(); }, 1, shadow);
                break;
            }

            case QEvent::Move:
                d->shadow->move(d->popup->pos() + d->widgetOffset);
                break;

            case QEvent::Resize:
                d->updateGeometry();
                break;

            default:
                break;
        }
    }

    return base_type::eventFilter(object, event);
}

} // namespace nx::vms::client::desktop
