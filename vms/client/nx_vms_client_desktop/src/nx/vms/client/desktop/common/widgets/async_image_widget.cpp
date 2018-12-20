

#include <QtWidgets/QLabel>
#include <QtWidgets/QStyle>

#include <client/client_globals.h>

#include <nx/vms/client/desktop/image_providers/camera_thumbnail_manager.h>
#include <nx/vms/client/desktop/common/widgets/async_image_widget.h>
#include <nx/vms/client/desktop/image_providers/image_provider.h>
#include <nx/client/core/utils/geometry.h>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <ui/style/helper.h>
#include <nx/vms/client/desktop/common/widgets/autoscaled_plain_text.h>
#include <nx/vms/client/desktop/common/widgets/busy_indicator.h>

#include <utils/common/scoped_painter_rollback.h>

namespace nx::vms::client::desktop {

namespace {

static const QMargins kMinIndicationMargins(4, 2, 4, 2);

// Default size must be big enough to avoid layout flickering.
static constexpr QSize kDefaultThumbnailSize(1920, 1080);

static constexpr bool kShowBusyIndicatorInInitialState = true;

} // namespace

// BusyIndicatorWidget draws dots snapped to the pixel grid.
// This descendant when it is downscaled draws dots generally not snapped.
class QnAutoscaledBusyIndicatorWidget: public BusyIndicatorWidget
{
    using base_type = BusyIndicatorWidget;

public:
    QnAutoscaledBusyIndicatorWidget(QWidget* parent = nullptr):
        base_type(parent)
    {
    }

    virtual void paint(QPainter* painter) override
    {
        auto sourceRect = indicatorRect();
        auto targetRect = contentsRect();

        qreal scale = core::Geometry::scaleFactor(sourceRect.size(), targetRect.size(),
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
    m_placeholder(new AutoscaledPlainText(this)),
    m_indicator(new QnAutoscaledBusyIndicatorWidget(this))
{
    retranslateUi();
    setBackgroundRole(QPalette::Window);
    setAttribute(Qt::WA_Hover);

    m_placeholder->setProperty(style::Properties::kDontPolishFontProperty, true);
    setProperty(style::Properties::kDontPolishFontProperty, true);

    m_placeholder->setAlignment(Qt::AlignCenter);
    m_placeholder->setContentsMargins(kMinIndicationMargins);
    m_placeholder->setHidden(true);
    anchorWidgetToParent(m_placeholder);

    m_indicator->setContentsMargins(kMinIndicationMargins);
    m_indicator->setBorderRole(QPalette::Window);
    anchorWidgetToParent(m_indicator);
}

AsyncImageWidget::~AsyncImageWidget()
{
}

ImageProvider* AsyncImageWidget::imageProvider() const
{
    return m_imageProvider.data();
}

void AsyncImageWidget::setImageProvider(ImageProvider* provider)
{
    if (m_imageProvider == provider)
        return;

    if (m_imageProvider)
        m_imageProvider->disconnect(this);

    m_imageProvider = provider;
    m_previousStatus = Qn::ThumbnailStatus::Invalid;

    if (m_imageProvider)
    {
        connect(m_imageProvider, &ImageProvider::imageChanged, this,
            &AsyncImageWidget::updateThumbnailImage);

        connect(m_imageProvider, &ImageProvider::statusChanged, this,
            &AsyncImageWidget::updateThumbnailStatus);

        connect(m_imageProvider, &ImageProvider::sizeHintChanged, this,
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

BusyIndicatorWidget* AsyncImageWidget::busyIndicator() const
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

QRectF AsyncImageWidget::highlightRect() const
{
    return m_highlightRect;
}

void AsyncImageWidget::setHighlightRect(const QRectF& relativeRect)
{
    const auto newRect = relativeRect.intersected(QRectF(0, 0, 1, 1));
    if (m_highlightRect == newRect)
        return;

    m_highlightRect = newRect;
    update();
}

AsyncImageWidget::CropMode AsyncImageWidget::cropMode() const
{
    return m_cropMode;
}

void AsyncImageWidget::setCropMode(CropMode value)
{
    if (m_cropMode == value)
        return;

    m_cropMode = value;
    update();
}

bool AsyncImageWidget::cropRequired() const
{
    if (m_highlightRect.isEmpty())
        return false;

    switch (m_cropMode)
    {
        case CropMode::never:
            return false;

        case CropMode::always:
            return true;

        case CropMode::hovered:
            return underMouse();

        case CropMode::notHovered:
            return !underMouse();
    }

    NX_ASSERT(false); //< Should never happen.
    return false;
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
        const auto paintSize = core::Geometry::scaled(m_preview.size(), size(), Qt::KeepAspectRatio);
        const auto paintRect = QStyle::alignedRect(layoutDirection(), Qt::AlignCenter,
            paintSize.toSize(), rect());

        QRectF highlightSubRect;
        painter.setRenderHints(QPainter::SmoothPixmapTransform);

        if (cropRequired())
        {
            const auto croppedImageRect = core::Geometry::subRect(m_preview.rect(), m_highlightRect);
            const auto boundingRect = core::Geometry::expanded(core::Geometry::aspectRatio(paintRect),
                croppedImageRect, Qt::KeepAspectRatioByExpanding, Qt::AlignCenter);

            const auto sourceRect = core::Geometry::movedInto(boundingRect, m_preview.rect());

            painter.drawPixmap(paintRect, m_preview, sourceRect);
            highlightSubRect = core::Geometry::toSubRect(sourceRect, croppedImageRect);
        }
        else
        {
            painter.drawPixmap(paintRect, m_preview);
            highlightSubRect = m_highlightRect;
        }

        if (highlightSubRect.isEmpty())
            return;

        // Dim everything around highlighted area.
        const auto highlightRect = core::Geometry::subRect(paintRect, highlightSubRect)
            .toAlignedRect().intersected(paintRect);

        if (highlightRect != paintRect)
        {
            QPainterPath path;
            path.addRegion(QRegion(paintRect).subtracted(highlightRect));
            painter.fillPath(path, palette().alternateBase());
        }

        // Paint frame.
        painter.setPen(QPen(palette().highlight(), 1, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        painter.drawRect(core::Geometry::eroded(QRectF(highlightRect), 0.5));
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
    return qMin(hint.height(), qRound(hint.height() / hint.width() * width));
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

void AsyncImageWidget::setAutoScaleDown(bool value)
{
    m_autoScaleDown = value;
}

bool AsyncImageWidget::autoScaleDown() const
{
    return m_autoScaleDown;
}

// Enable automatic upscaling of the image up to widget's size
void AsyncImageWidget::setAutoScaleUp(bool value)
{
    m_autoScaleUp = value;
}

bool AsyncImageWidget::autoScaleUp() const
{
    return m_autoScaleUp;
}

AsyncImageWidget::ReloadMode AsyncImageWidget::reloadMode() const
{
    return m_reloadMode;
}

void AsyncImageWidget::setReloadMode(ReloadMode value)
{
    m_reloadMode = value;
}

void AsyncImageWidget::setNoDataMode(bool noData)
{
    m_noDataMode = noData;
    updateThumbnailStatus(m_previousStatus);
}

void AsyncImageWidget::updateThumbnailStatus(Qn::ThumbnailStatus status)
{
    if (m_noDataMode)
    {
        m_placeholder->show();
        m_indicator->hide();
    }
    else
    {
        switch (status)
        {
            case Qn::ThumbnailStatus::Loaded:
                m_placeholder->hide();
                m_indicator->hide();
                break;

            case Qn::ThumbnailStatus::Loading:
                if (m_reloadMode == ReloadMode::showLoadingIndicator
                    || m_previousStatus == Qn::ThumbnailStatus::Invalid)
                {
                    m_placeholder->hide();
                    m_indicator->show();
                }
                break;

            case Qn::ThumbnailStatus::Refreshing:
                break;

            case Qn::ThumbnailStatus::Invalid:
                m_placeholder->hide();
                m_indicator->setVisible(kShowBusyIndicatorInInitialState);
                break;

            case Qn::ThumbnailStatus::NoData:
                m_placeholder->show();
                m_indicator->hide();
                break;
        }
    }
    m_previousStatus = status;
}

void AsyncImageWidget::updateThumbnailImage(const QImage& image)
{
    QSize sourceSize = image.size();
    const auto maxHeight = qMin(maximumHeight(), heightForWidth(maximumWidth()));

    bool scale = false;
    QSize targetSize;

    if (sourceSize.height() > maxHeight && m_autoScaleDown)
    {
        scale = true;
        targetSize = maximumSize();
    }

    if (sourceSize.height() < maxHeight && m_autoScaleUp)
    {
        scale = true;
        targetSize = maximumSize();
    }

    m_preview = QPixmap::fromImage(scale
        ? image.scaled(maximumSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation)
        : image);

    invalidateGeometry();
    update();
}

} // namespace nx::vms::client::desktop

