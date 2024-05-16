// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "async_image_widget.h"

#include <QtCore/QTimer>
#include <QtCore/QtMath>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStyle>

#include <client/client_globals.h>
#include <core/resource/camera_resource.h>
#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/math/math.h>
#include <nx/vms/client/core/image_providers/camera_thumbnail_manager.h>
#include <nx/vms/client/core/image_providers/camera_thumbnail_provider.h>
#include <nx/vms/client/core/image_providers/image_provider.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <nx/vms/client/desktop/common/widgets/autoscaled_plain_text.h>
#include <nx/vms/client/desktop/common/widgets/busy_indicator.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <ui/common/palette.h>
#include <ui/utils/blur_image.h>
#include <utils/common/scoped_painter_rollback.h>

namespace nx::vms::client::desktop {

namespace {

static const QMargins kMinIndicationMargins(4, 2, 4, 2);

// Default size must be big enough to avoid layout flickering.
static constexpr QSize kDefaultThumbnailSize(1920, 1080);

static constexpr bool kShowLoadingIndicationInInitialState = true;

// BusyIndicatorWidget draws dots snapped to the pixel grid.
// This descendant when it is downscaled draws dots generally not snapped.
class AutoscaledBusyIndicatorWidget: public BusyIndicatorWidget
{
    using base_type = BusyIndicatorWidget;

public:
    AutoscaledBusyIndicatorWidget(QWidget* parent = nullptr):
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

} // namespace

AsyncImageWidget::AsyncImageWidget(QWidget* parent):
    base_type(parent),
    m_placeholder(new AutoscaledPlainText(this)),
    m_indicator(new AutoscaledBusyIndicatorWidget(this))
{
    setPaletteColor(this, QPalette::Window, core::colorTheme()->color("dark6"));
    setPaletteColor(this, QPalette::WindowText, core::colorTheme()->color("light16"));
    setPaletteColor(this, QPalette::Highlight, core::colorTheme()->color("brand_core"));
    setPaletteColor(this, QPalette::AlternateBase, core::colorTheme()->color("dark1", 110));

    retranslateUi();
    setBackgroundRole(QPalette::Window);
    setAutoFillBackground(true);
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

core::ImageProvider* AsyncImageWidget::imageProvider() const
{
    return m_imageProvider.data();
}

void AsyncImageWidget::setImageProvider(core::ImageProvider* provider)
{
    if (m_imageProvider == provider)
        return;

    if (m_imageProvider)
        m_imageProvider->disconnect(this);

    m_imageProvider = provider;
    m_previousStatus = core::ThumbnailStatus::Invalid;

    if (m_imageProvider)
    {
        connect(m_imageProvider, &core::ImageProvider::imageChanged, this,
            &AsyncImageWidget::updateCache);

        connect(m_imageProvider, &core::ImageProvider::statusChanged, this,
            &AsyncImageWidget::updateThumbnailStatus);

        connect(m_imageProvider, &core::ImageProvider::sizeHintChanged, this,
            &AsyncImageWidget::updateCache);

        connect(m_imageProvider, &QObject::destroyed, this,
            [this]
            {
                updateCache();
                updateThumbnailStatus(core::ThumbnailStatus::Invalid);
            });
    }

    updateCache();
    updateThumbnailStatus(m_imageProvider
        ? m_imageProvider->status()
        : core::ThumbnailStatus::Invalid);
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
    if (m_forceNoCrop || m_highlightRect.isEmpty())
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

QTransform AsyncImageWidget::getAdjustment(QRectF target, QRectF image)
{
    qreal dx = 0.0;
    qreal dy = 0.0;
    // Horizontal correction
    if (image.left() < target.left() && image.right() < target.right())
    {
        dx = image.width() > target.width()
            ? target.right() - image.right() //< Align right edges.
            : target.left() - image.left(); //< Align left edges.
    }
    else if (image.right() > target.right() && image.left() > target.left())
    {
        dx = image.width() > target.width()
            ? target.left() - image.left() //< Align left edges.
            : target.right() - image.right(); //< Align right edges.
    }
    // Vertical correction
    if (image.top() < target.top() && image.bottom() < target.bottom())
    {
        dy = image.height() > target.height()
            ? target.bottom() - image.bottom() //< Align bottom edges.
            : target.top() - image.top(); //< Align top edges.
    }
    else if (image.bottom() > target.bottom() && image.top() > target.top())
    {
        dy = image.height() > target.height()
            ? target.top() - image.top() //< Align top edges.
            : target.bottom() - image.bottom(); //< Align bottom edges.
    }
    return QTransform::fromTranslate(dx, dy);
}

void AsyncImageWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHints(QPainter::SmoothPixmapTransform);

    if (m_placeholder->isVisible() && m_placeholderBackgroundColor.isValid())
        painter.fillRect(m_placeholder->geometry(), m_placeholderBackgroundColor);

    QRect targetImageRect; //< Image rectangle in target coordinates.
    QRect targetHighlightRect; //< Highlight rectangle in target coordinates.
    const bool croppedMode = cropRequired();

    if (!m_preview.isNull() && !m_placeholder->isVisible())
    {
        QRectF sourceRect = m_preview.rect();
        const QRectF sourceHighlightRect = core::Geometry::subRect(sourceRect, m_highlightRect);
        if (croppedMode)
            sourceRect = sourceHighlightRect;

        QRectF targetRect = rect();
        const qreal scaleFactor = core::Geometry::scaleFactor(sourceRect.size(), size());
        qreal actualScale = 1.0;
        if (autoScaleUp() && scaleFactor > 1)
            actualScale = scaleFactor;
        if (autoScaleDown() && scaleFactor < 1)
            actualScale = scaleFactor;

        QTransform transform; // Transformations are applied in reversed order.
        // 3. Return (0,0) to target center.
        transform.translate(targetRect.center().x(), targetRect.center().y());
        // 2. Scale
        transform.scale(actualScale, actualScale);
        // 1. Move source rectangle center to (0,0).
        transform.translate(-sourceRect.center().x(), -sourceRect.center().y());

        targetImageRect = transform.mapRect(m_preview.rect());
        // Adjust transformation if targetImageRect is not inside targetRect to get better display.
        const QTransform adjustment = getAdjustment(targetRect, targetImageRect);
        // 4. Adjust main transformation.
        transform = transform * adjustment;

        targetImageRect = transform.mapRect(m_preview.rect()); //< transform probably was adjusted.
        targetHighlightRect = transform.mapRect(sourceHighlightRect).toRect();

        // Draw main image.
        if (m_applyBlurEffect)
        {
            // We can't use QGraphicsBlurEffect because it does not work when the widget
            // embedded into QGraphicsProxyWidget.
            QImage image = m_previewImage;
            qt_blurImage(image, /*radius*/ 15 / actualScale, /*improvedQuality*/ false);
            painter.drawImage(targetImageRect, image);
        }
        else
        {
            painter.drawPixmap(targetImageRect, m_preview);
        }
    }

    // For simplification, in current implementation the border covers 1-pixel frame boundary.
    if (m_borderRole != QPalette::NoRole)
    {
        painter.setPen(palette().color(m_borderRole));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));
    }

    // Draw highlight
    if (!targetHighlightRect.isEmpty() && !m_forceNoCrop)
    {
        // Dim everything around highlighted area.
        if (targetImageRect != targetHighlightRect)
        {
            QPainterPath path;
            path.addRegion(QRegion(targetImageRect).subtracted(targetHighlightRect));
            painter.fillPath(path, palette().alternateBase());
        }

        if (!croppedMode)
        {
            // Paint highlight frame.
            painter.setPen(
                QPen(palette().highlight(), 1, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
            painter.drawRect(core::Geometry::eroded(QRectF(targetHighlightRect), 0.5));
        }
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
        updateSizeHint();

    return m_cachedSizeHint;
}

bool AsyncImageWidget::hasHeightForWidth() const
{
    return true;
}

int AsyncImageWidget::heightForWidth(int width) const
{
    const QSizeF hint = sizeHint();
    qreal height = hint.height();

    if (autoScaleDown())
        height = qMin(height, qCeil(width / hint.width() * hint.height()));

    if (autoScaleUp())
        height = qMax(height, qFloor(width / hint.width() * hint.height()));

    height = qRound(height);

    static constexpr qreal kDefaultRatio = 16.0 / 9.0;
    if (qAbs(width / height - kDefaultRatio) < 0.05)
        height = qRound(width / kDefaultRatio);

    return height;
}

void AsyncImageWidget::retranslateUi()
{
    QString text = m_placeholderText;

    if (text.isEmpty())
        text = ini().debugDisableCameraThumbnails ? tr("DISABLED") : tr("NO DATA");

    m_placeholder->setText(text);
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
    if (m_reloadMode != ReloadMode::showLoadingIndicator
        && m_reloadMode != ReloadMode::showBlurredPreviousImage)
    {
        m_indicator->hide();
    }
}

void AsyncImageWidget::setPlaceholder(const QString& text)
{
    m_placeholderText = text;

    retranslateUi();
    updateThumbnailStatus(m_previousStatus);
}

const QString& AsyncImageWidget::placeholder() const
{
    return m_placeholderText;
}

void AsyncImageWidget::setForceLoadingIndicator(bool force)
{
    m_forceLoadingIndicator = force;

    updateThumbnailStatus(m_previousStatus);
}

void AsyncImageWidget::updateSizeHint() const
{
    QSize perfectSize;

    if (m_imageProvider)
    {
        perfectSize = m_imageProvider->image().size(); //< Take image size if possible.
        if (perfectSize.isEmpty())
            perfectSize = m_imageProvider->sizeHint(); //< Take sizeHint if possible.
    }

    if (perfectSize.isEmpty())
        perfectSize = minimumSize(); //< Take minimumSize if possible.

    if (perfectSize.isEmpty())
        perfectSize = kDefaultThumbnailSize; //< Take a constant.

    m_cachedSizeHint = perfectSize;

    // Decrease sizeHint if it does not fit in maximumSize() and autoScaleDown.
    if (autoScaleDown())
    {
        const auto oversizeCoefficient = qMax(
            static_cast<qreal>(m_cachedSizeHint.width()) / maximumSize().width(),
            static_cast<qreal>(m_cachedSizeHint.height()) / maximumSize().height());

        if (oversizeCoefficient > 1.0)
        {
            m_cachedSizeHint.setWidth(qMax(1, qRound(perfectSize.width() / oversizeCoefficient)));
            m_cachedSizeHint.setHeight(qMax(1, qRound(perfectSize.height() / oversizeCoefficient)));
        }
    }
}

void AsyncImageWidget::updateCache()
{
    m_preview = QPixmap();
    m_previewImage = m_imageProvider ? m_imageProvider->image() : QImage();
    if (!m_previewImage.isNull())
        m_preview = QPixmap::fromImage(m_previewImage);

    m_forceNoCrop = QnLexical::deserialized<bool>(
        m_previewImage.text(core::CameraThumbnailProvider::kFrameFromPluginKey));

    // Update geometry if we got new sizeHint value (probably due to new image).
    const QSize oldSizeHint = m_cachedSizeHint;
    updateSizeHint();
    if (m_cachedSizeHint != oldSizeHint)
        updateGeometry();

    update();
}

void AsyncImageWidget::updateThumbnailStatus(core::ThumbnailStatus status)
{
    if (m_forceLoadingIndicator)
    {
        setLoadingIndicationVisible(true);
        m_placeholder->hide();
    }
    else if (!m_placeholderText.isEmpty())
    {
        setLoadingIndicationVisible(false);
        m_placeholder->show();
    }
    else
    {
        switch (status)
        {
            case core::ThumbnailStatus::Loaded:
                setLoadingIndicationVisible(false);
                m_placeholder->hide();
                break;

            case core::ThumbnailStatus::Loading:
                setLoadingIndicationVisible(true);
                m_placeholder->hide();
                break;

            case core::ThumbnailStatus::Refreshing:
                break;

            case core::ThumbnailStatus::Invalid:
                setLoadingIndicationVisible(kShowLoadingIndicationInInitialState);
                m_placeholder->hide();
                break;

            case core::ThumbnailStatus::NoData:
                setLoadingIndicationVisible(false);
                m_placeholder->show();
                break;
        }
    }
    m_previousStatus = status;
}

void AsyncImageWidget::setLoadingIndicationVisible(bool visible)
{
    switch (m_reloadMode)
    {
        case ReloadMode::showLoadingIndicator:
            m_indicator->setVisible(visible);
            break;

        case ReloadMode::showPreviousImage:
            break;

        case ReloadMode::showBlurredPreviousImage:
            m_indicator->setVisible(visible && m_preview.isNull());
            if (m_applyBlurEffect != visible)
            {
                m_applyBlurEffect = visible;
                update();
            }
            break;
    }
}

} // namespace nx::vms::client::desktop
