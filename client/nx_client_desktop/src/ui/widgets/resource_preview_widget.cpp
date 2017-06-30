#include "resource_preview_widget.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QStyle>

#include <client/client_globals.h>

#include <camera/camera_thumbnail_manager.h>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/common/geometry.h>
#include <ui/style/helper.h>
#include <ui/widgets/common/autoscaled_plain_text.h>
#include <ui/widgets/common/busy_indicator.h>

#include <utils/image_provider.h>
#include <utils/common/scoped_painter_rollback.h>

namespace {

static const QMargins kMinIndicationMargins(4, 2, 4, 2);
static const QSize kFrameSize(2, 2);

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

} // namespace

QnResourcePreviewWidget::QnResourcePreviewWidget(QWidget* parent /*= nullptr*/) :
    base_type(parent),
    m_preview(new QLabel(this)),
    m_placeholder(new QnAutoscaledPlainText(this)),
    m_indicator(new QnAutoscaledBusyIndicatorWidget(this)),
    m_pages(new QStackedWidget(this))
{
    m_pages->addWidget(m_preview);
    m_pages->addWidget(m_placeholder);
    m_pages->addWidget(m_indicator);
    m_pages->setCurrentWidget(m_indicator);
    m_pages->setFrameStyle(QFrame::NoFrame | QFrame::Plain);

    retranslateUi();

    m_placeholder->setProperty(style::Properties::kDontPolishFontProperty, true);
    setProperty(style::Properties::kDontPolishFontProperty, true);

    m_placeholder->setAlignment(Qt::AlignCenter);
    m_placeholder->setContentsMargins(kMinIndicationMargins);

    m_indicator->setContentsMargins(kMinIndicationMargins);

    m_preview->setAlignment(Qt::AlignCenter);
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

void QnResourcePreviewWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setPen(palette().window().color());
    painter.setBrush(palette().dark());

    QRect content = QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, sizeHint(), rect());
    painter.drawRect(content.intersected(rect()).adjusted(0, 0, -1, -1));
}

void QnResourcePreviewWidget::resizeEvent(QResizeEvent* event)
{
    m_pages->resize(size());
    base_type::resizeEvent(event);
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
        if (m_preview->pixmap())
            m_cachedSizeHint = m_preview->pixmap()->size();

        if (m_cachedSizeHint.isEmpty())
        {
            if (m_imageProvider)
                m_cachedSizeHint = m_imageProvider->sizeHint();

            if (m_cachedSizeHint.isNull())
                m_cachedSizeHint = minimumSize();
        }

        const QSize maxSize = maximumSize() - kFrameSize;

        qreal oversizeCoefficient = qMax(
            static_cast<qreal>(m_cachedSizeHint.width()) / maxSize.width(),
            static_cast<qreal>(m_cachedSizeHint.height()) / maxSize.height());

        if (oversizeCoefficient > 1.0)
            m_cachedSizeHint /= oversizeCoefficient;

        m_cachedSizeHint += kFrameSize;
    }

    return m_cachedSizeHint;
}

QSize QnResourcePreviewWidget::minimumSizeHint() const
{
    return sizeHint();
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
            m_pages->setCurrentWidget(m_preview);
            break;

        case Qn::ThumbnailStatus::NoData:
            m_pages->setCurrentWidget(m_placeholder);
            break;

        default:
            m_pages->setCurrentWidget(m_indicator);
            break;
    }
}

void QnResourcePreviewWidget::updateThumbnailImage(const QImage& image)
{
    m_preview->setPixmap(QPixmap::fromImage(image));
    invalidateGeometry();
}
