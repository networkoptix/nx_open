#include "resource_preview_widget.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QStackedWidget>

#include <camera/camera_thumbnail_manager.h>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/common/geometry.h>
#include <ui/style/helper.h>
#include <ui/widgets/common/autoscaled_plain_text.h>
#include <ui/widgets/common/busy_indicator.h>

#include <utils/common/scoped_painter_rollback.h>

namespace {

    const qreal kDefaultAspectRatio = 4.0 / 3.0;
    const QMargins kMinIndicationMargins(4, 2, 4, 2);

    /* QnBusyIndicatorWidget draws dots aligned to the pixel grid.
     * This descendant does not, when it is downscaled. */
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

            qreal scale = qMin(
                static_cast<qreal>(targetRect.width()) / sourceRect.width(),
                static_cast<qreal>(targetRect.height()) / sourceRect.height());

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
    m_thumbnailManager(new QnCameraThumbnailManager()),
    m_target(),
    m_cachedSizeHint(),
    m_resolutionHint(),
    m_aspectRatio(kDefaultAspectRatio),
    m_preview(new QLabel(this)),
    m_placeholder(new QnAutoscaledPlainText(this)),
    m_indicator(new QnAutoscaledBusyIndicatorWidget(this)),
    m_pages(new QStackedWidget(this)),
    m_status(QnCameraThumbnailManager::None)
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

    connect(m_thumbnailManager, &QnCameraThumbnailManager::thumbnailReady, this,
        [this](const QnUuid& resourceId, const QPixmap& thumbnail)
        {
            if (!m_target || m_target->getId() != resourceId)
                return;

            if (m_status != QnCameraThumbnailManager::Loaded)
                return;

            m_pages->setCurrentWidget(m_preview);
            m_preview->setPixmap(thumbnail);
            m_cachedSizeHint = QSize();
            updateGeometry();
        });

    connect(m_thumbnailManager, &QnCameraThumbnailManager::statusChanged, this,
        [this](const QnUuid& resourceId, QnCameraThumbnailManager::ThumbnailStatus status)
        {
            if (!m_target || m_target->getId() != resourceId)
                return;

            m_status = status;
            switch (m_status)
            {
                case QnCameraThumbnailManager::Loaded:
                    /* This is handled in thumbnailReady handler */
                    break;

                case QnCameraThumbnailManager::NoData:
                    m_pages->setCurrentWidget(m_placeholder);
                    break;

                default:
                    m_pages->setCurrentWidget(m_indicator);
                    break;
            }
        });
}

QnResourcePreviewWidget::~QnResourcePreviewWidget()
{
}

const QnResourcePtr& QnResourcePreviewWidget::targetResource() const
{
    return m_target;
}

void QnResourcePreviewWidget::setTargetResource(const QnResourcePtr& target)
{
    auto camera = target.dynamicCast<QnVirtualCameraResource>();

    if (camera == m_target)
        return;

    m_target = camera;
    m_resolutionHint = QSize();
    m_aspectRatio = kDefaultAspectRatio;

    if (camera)
    {
        m_thumbnailManager->selectResource(camera);

        auto customAspectRatio = camera->aspectRatio();
        if (customAspectRatio.isValid())
            m_aspectRatio = customAspectRatio.toFloat();
    }

    m_preview->setPixmap(QPixmap());
    m_cachedSizeHint = QSize();
    updateGeometry();
}

QSize QnResourcePreviewWidget::thumbnailSize() const
{
    return m_thumbnailManager->thumbnailSize();
}

void QnResourcePreviewWidget::setThumbnailSize(const QSize &size)
{
    m_thumbnailManager->setThumbnailSize(size);
    m_cachedSizeHint = QSize();
    updateGeometry();
}

QnBusyIndicatorWidget* QnResourcePreviewWidget::busyIndicator() const
{
    return m_indicator;
}

void QnResourcePreviewWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
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
            m_cachedSizeHint = thumbnailSize();
            if (m_cachedSizeHint.isNull())
            {
                m_cachedSizeHint = minimumSize();
            }
            else
            {
                if (m_cachedSizeHint.height() == 0)
                    m_cachedSizeHint.setHeight(static_cast<int>(m_cachedSizeHint.width() / m_aspectRatio));
                else if (m_cachedSizeHint.width() == 0)
                    m_cachedSizeHint.setWidth(static_cast<int>(m_cachedSizeHint.height() * m_aspectRatio));
            }
        }

        static const QSize kFrameSize(2, 2);
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
