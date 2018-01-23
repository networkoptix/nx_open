

#include <QtWidgets/QLabel>
#include <QtWidgets/QStyle>

#include <client/client_globals.h>

#include <nx/client/desktop/image_providers/camera_thumbnail_manager.h>
#include <nx/client/desktop/common/widgets/async_image_widget.h>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/common/geometry.h>
#include <ui/common/widget_anchor.h>
#include <ui/style/helper.h>
#include <ui/widgets/common/autoscaled_plain_text.h>
#include <ui/widgets/common/busy_indicator.h>

#include <nx/client/desktop/image_providers/image_provider.h>
#include <utils/common/scoped_painter_rollback.h>

namespace nx {
namespace client {
namespace desktop {

namespace
{
    static const QMargins kMinIndicationMargins(4, 2, 4, 2);

    // Default size must be big enough to avoid layout flickering.
    static const QSize kDefaultThumbnailSize(1920, 1080);
}

/* QnBusyIndicatorWidget draws dots snapped to the pixel grid.
 * This descendant when it is downscaled draws dots generally not snapped. */
class QnAutoscaledBusyIndicatorWidget: public QnBusyIndicatorWidget
{
    using base_type = QnBusyIndicatorWidget;

public:
    QnAutoscaledBusyIndicatorWidget(QWidget* parent = nullptr):
        base_type(parent)
    {
    }

    virtual void paint(QPainter* painter) override
    {
        auto sourceRect = indicatorRect();
        auto targetRect = contentsRect();

        qreal scale = QnGeometry::scaleFactor(sourceRect.size(), targetRect.size(),
            Qt::KeepAspectRatio);

        QnScopedPainterTransformRollback transformRollback(painter);
        if (scale < 1.0)
        {
            painter->translate(targetRect.center());
            painter->scale(scale, scale);
            painter->translate(-targetRect.center());
        }

        base_type::paint(painter);
    }
};

AsyncImageWidget::AsyncImageWidget(QWidget* parent):
    base_type(parent),
    m_placeholder(new QnAutoscaledPlainText(this)),
    m_indicator(new QnAutoscaledBusyIndicatorWidget(this))
{
    retranslateUi();
    setBackgroundRole(QPalette::Window);

    m_placeholder->setProperty(style::Properties::kDontPolishFontProperty, true);
    setProperty(style::Properties::kDontPolishFontProperty, true);

    m_placeholder->setAlignment(Qt::AlignCenter);
    m_placeholder->setContentsMargins(kMinIndicationMargins);
    m_placeholder->setHidden(true);
    new QnWidgetAnchor(m_placeholder);

    m_indicator->setContentsMargins(kMinIndicationMargins);
    m_indicator->setBorderRole(QPalette::Window);
    new QnWidgetAnchor(m_indicator);
}

AsyncImageWidget::~AsyncImageWidget()
{
}

QnImageProvider* AsyncImageWidget::imageProvider() const
{
    return m_imageProvider.data();
}

void AsyncImageWidget::setImageProvider(QnImageProvider* provider)
{
    if (m_imageProvider == provider)
        return;

    if (m_imageProvider)
        m_imageProvider->disconnect(this);

    m_imageProvider = provider;

    if (m_imageProvider)
    {
        connect(m_imageProvider, &QnImageProvider::imageChanged, this,
            &AsyncImageWidget::updateThumbnailImage);

        connect(m_imageProvider, &QnImageProvider::statusChanged, this,
            &AsyncImageWidget::updateThumbnailStatus);

        connect(m_imageProvider, &QnImageProvider::sizeHintChanged, this,
            &AsyncImageWidget::invalidateGeometry);

        connect(m_imageProvider, &QObject::destroyed, this,
            [this]
            {
                setImageProvider(nullptr);
            });
    }

    updateThumbnailImage(m_imageProvider ? m_imageProvider->image() : QImage());
    updateThumbnailStatus(m_imageProvider ? m_imageProvider->status() : Qn::ThumbnailStatus::Invalid);
    invalidateGeometry();
}

QnBusyIndicatorWidget* AsyncImageWidget::busyIndicator() const
{
    return m_indicator;
}

QPalette::ColorRole AsyncImageWidget::borderRole() const
{
    return m_borderRole;
}

void AsyncImageWidget::setBorderRole(QPalette::ColorRole role)
{
    if (m_borderRole == role)
        return;

    m_borderRole = role;
    update();
}

void AsyncImageWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    if (m_preview.isNull() || m_placeholder->isVisible())
    {
        QRectF paintRect = rect();
        painter.setBrush(palette().brush(backgroundRole()));

        if (m_borderRole == QPalette::NoRole)
        {
            painter.setPen(Qt::NoPen);
        }
        else
        {
            painter.setPen(palette().color(m_borderRole));
            paintRect.adjust(0.5, 0.5, -0.5, -0.5);
        }

        painter.drawRect(paintRect);
    }
    else
    {
        const auto sourceSize = m_preview.size() / m_preview.devicePixelRatio();
        const auto paintSize = QnGeometry::bounded(sourceSize, size(), Qt::KeepAspectRatio);
        const auto paintRect = QStyle::alignedRect(layoutDirection(), Qt::AlignCenter,
            size(), QRect(QPoint(), paintSize.toSize()));
        painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        painter.drawPixmap(paintRect, m_preview);
    }
}

void AsyncImageWidget::changeEvent(QEvent* event)
{
    switch (event->type())
    {
        case QEvent::LanguageChange:
            retranslateUi();
            break;

        case QEvent::FontChange:
            m_placeholder->setFont(font());
            break;

        default:
            break;
    }

    base_type::changeEvent(event);
}

QSize AsyncImageWidget::sizeHint() const
{
    if (!m_cachedSizeHint.isValid())
    {
        if (!m_preview.isNull())
            m_cachedSizeHint = m_preview.size();

        if (m_cachedSizeHint.isEmpty())
        {
            if (m_imageProvider)
                m_cachedSizeHint = m_imageProvider->sizeHint();

            if (m_cachedSizeHint.isNull())
                m_cachedSizeHint = minimumSize();

            if (m_cachedSizeHint.isNull())
                m_cachedSizeHint = kDefaultThumbnailSize;
        }

        const QSize maxSize = maximumSize();

        qreal oversizeCoefficient = qMax(
            static_cast<qreal>(m_cachedSizeHint.width()) / maxSize.width(),
            static_cast<qreal>(m_cachedSizeHint.height()) / maxSize.height());

        if (oversizeCoefficient > 1.0)
            m_cachedSizeHint /= oversizeCoefficient;
    }

    return m_cachedSizeHint;
}

bool AsyncImageWidget::hasHeightForWidth() const
{
    return true;
}

int AsyncImageWidget::heightForWidth(int width) const
{
    const QSizeF hint = sizeHint();
    return qRound(hint.height() / hint.width() * width);
}

void AsyncImageWidget::retranslateUi()
{
    m_placeholder->setText(tr("NO DATA"));
}

void AsyncImageWidget::invalidateGeometry()
{
    m_cachedSizeHint = QSize();
    updateGeometry();
}

void AsyncImageWidget::updateThumbnailStatus(Qn::ThumbnailStatus status)
{
    switch (status)
    {
        case Qn::ThumbnailStatus::Loaded:
            m_placeholder->setHidden(true);
            m_indicator->setHidden(true);
            break;

        case Qn::ThumbnailStatus::Loading:
            m_placeholder->setHidden(true);
            m_indicator->setHidden(false);
            break;

        default:
            m_placeholder->setHidden(false);
            m_indicator->setHidden(true);
            break;
    }
}

void AsyncImageWidget::updateThumbnailImage(const QImage& image)
{
    const auto maxHeight = qMin(maximumHeight(), heightForWidth(maximumWidth()));
    m_preview = QPixmap::fromImage(image.size().height() > maxHeight
        ? image.scaled(maximumSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation)
        : image);

    invalidateGeometry();
    update();
}

} // namespace desktop
} // namespace client
} // namespace nx

