#include "resource_preview_widget.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QStyle>

#include <client/client_globals.h>

#include <camera/camera_thumbnail_manager.h>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/client/core/utils/geometry.h>
#include <ui/common/widget_anchor.h>
#include <ui/style/helper.h>
#include <ui/widgets/common/autoscaled_plain_text.h>
#include <ui/widgets/common/busy_indicator.h>

#include <utils/image_provider.h>
#include <utils/common/scoped_painter_rollback.h>

using nx::client::core::Geometry;

namespace {

static const QMargins kMinIndicationMargins(4, 2, 4, 2);

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

        qreal scale = Geometry::scaleFactor(sourceRect.size(), targetRect.size(),
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

QnResourcePreviewWidget::QnResourcePreviewWidget(QWidget* parent):
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

QnResourcePreviewWidget::~QnResourcePreviewWidget()
{
}

QnImageProvider* QnResourcePreviewWidget::imageProvider() const
{
    return m_imageProvider.data();
}

void QnResourcePreviewWidget::setImageProvider(QnImageProvider* provider)
{
    if (m_imageProvider == provider)
        return;

    if (m_imageProvider)
        m_imageProvider->disconnect(this);

    m_imageProvider = provider;

    if (m_imageProvider)
    {
        connect(m_imageProvider, &QnImageProvider::imageChanged, this,
            &QnResourcePreviewWidget::updateThumbnailImage);

        connect(m_imageProvider, &QnImageProvider::statusChanged, this,
            &QnResourcePreviewWidget::updateThumbnailStatus);

        connect(m_imageProvider, &QnImageProvider::sizeHintChanged, this,
            &QnResourcePreviewWidget::invalidateGeometry);

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

QnBusyIndicatorWidget* QnResourcePreviewWidget::busyIndicator() const
{
    return m_indicator;
}

QPalette::ColorRole QnResourcePreviewWidget::borderRole() const
{
    return m_borderRole;
}

void QnResourcePreviewWidget::setBorderRole(QPalette::ColorRole role)
{
    if (m_borderRole == role)
        return;

    m_borderRole = role;
    update();
}

QRectF QnResourcePreviewWidget::highlight() const
{
    return m_highlight;
}

void QnResourcePreviewWidget::setHighlight(const QRectF& relativeRect)
{
    if (m_highlight == relativeRect)
        return;

    m_highlight = relativeRect;
    update();
}

void QnResourcePreviewWidget::paintEvent(QPaintEvent* /*event*/)
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
        const auto paintSize = Geometry::bounded(sourceSize, size(), Qt::KeepAspectRatio);
        const auto paintRect = QStyle::alignedRect(layoutDirection(), Qt::AlignCenter,
            paintSize.toSize(), QRect(QPoint(), size()));
        painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        painter.drawPixmap(paintRect, m_preview);

        if (m_highlight.isEmpty())
            return;

        // Dim everything around highlighted area.
        QPainterPath path;
        path.addRegion(QRegion(paintRect).subtracted(
            Geometry::subRect(paintRect, m_highlight).toAlignedRect()));
        static const auto kDimmerColor = QColor("#70000000"); //< TODO: #vkutin Customize.
        painter.fillPath(path, kDimmerColor);
    }
}

void QnResourcePreviewWidget::changeEvent(QEvent* event)
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

QSize QnResourcePreviewWidget::sizeHint() const
{
    if (!m_cachedSizeHint.isValid())
    {
        if (!m_preview.isNull())
            m_cachedSizeHint = m_preview.size() / m_preview.devicePixelRatio();

        if (m_cachedSizeHint.isEmpty())
        {
            if (m_imageProvider)
                m_cachedSizeHint = m_imageProvider->sizeHint();

            if (m_cachedSizeHint.isNull())
                m_cachedSizeHint = minimumSize();
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

bool QnResourcePreviewWidget::hasHeightForWidth() const
{
    return true;
}

int QnResourcePreviewWidget::heightForWidth(int width) const
{
    const QSizeF hint = sizeHint();
    return qMin(hint.height(), qRound(hint.height() / hint.width() * width));
}

void QnResourcePreviewWidget::retranslateUi()
{
    m_placeholder->setText(tr("NO DATA"));
}

void QnResourcePreviewWidget::invalidateGeometry()
{
    m_cachedSizeHint = QSize();
    updateGeometry();
}

void QnResourcePreviewWidget::updateThumbnailStatus(Qn::ThumbnailStatus status)
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

void QnResourcePreviewWidget::updateThumbnailImage(const QImage& image)
{
    const auto maxHeight =
        qMin(maximumHeight(), maximumWidth() / Geometry::aspectRatio(sizeHint()));
    m_preview = QPixmap::fromImage(image.size().height() > maxHeight
        ? image.scaled(maximumSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation)
        : image);

    invalidateGeometry();
    update();
}
