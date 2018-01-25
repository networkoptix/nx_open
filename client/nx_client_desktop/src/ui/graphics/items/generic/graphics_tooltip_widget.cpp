#include "graphics_tooltip_widget.h"

#include <QtWidgets/QGraphicsLinearLayout>
#include <QtWidgets/QBoxLayout>

#include <ui/graphics/items/generic/masked_proxy_widget.h>
#include <ui/processors/clickable.h>
#include <ui/style/helper.h>
#include <ui/widgets/common/text_edit_label.h>
#include <ui/widgets/common/busy_indicator.h>
#include <nx/client/desktop/common/widgets/async_image_widget.h>

namespace {

static const int kNoDataFontPixelSize = 32;
static const int kNoDataFontWeight = QFont::Light;

} // namespace

QnGraphicsToolTipWidget::QnGraphicsToolTipWidget(QGraphicsItem* parent):
    base_type(parent),
    m_proxyWidget(new Clickable<QnMaskedProxyWidget>(this)),
    m_embeddedWidget(new QWidget()),
    m_textLabel(new QnTextEditLabel(m_embeddedWidget)),
    m_previewWidget(new nx::client::desktop::AsyncImageWidget(m_embeddedWidget))
{
    m_proxyWidget->setVisible(false);
    m_proxyWidget->setWidget(m_embeddedWidget);
    m_proxyWidget->installSceneEventFilter(this);

    m_embeddedWidget->setAttribute(Qt::WA_TranslucentBackground);

    auto dots = m_previewWidget->busyIndicator()->dots();
    dots->setDotRadius(style::Metrics::kStandardPadding / 2.0);
    dots->setDotSpacing(style::Metrics::kStandardPadding);

    auto layout = new QVBoxLayout(m_embeddedWidget);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_previewWidget);
    layout->addWidget(m_textLabel);

    auto graphicsLayout = new QGraphicsLinearLayout(Qt::Vertical);
    graphicsLayout->setContentsMargins(0, 0, 0, 0);
    graphicsLayout->addItem(m_proxyWidget);
    setLayout(graphicsLayout);

    QFont font;
    font.setPixelSize(kNoDataFontPixelSize);
    font.setWeight(kNoDataFontWeight);
    m_previewWidget->setFont(font);

    setThumbnailVisible(false);
}

bool QnGraphicsToolTipWidget::sceneEventFilter(QGraphicsItem* watched, QEvent* event)
{
    if (watched == m_proxyWidget
        && event->type() == QEvent::GraphicsSceneMousePress
        && static_cast<QGraphicsSceneMouseEvent*>(event)->button() == Qt::LeftButton)
    {
        thumbnailClicked();
    }

    return base_type::sceneEventFilter(watched, event);
}

void QnGraphicsToolTipWidget::forceLayoutUpdate()
{
    m_embeddedWidget->layout()->activate();
}

QString QnGraphicsToolTipWidget::text() const
{
    return m_textLabel->toPlainText();
}

void QnGraphicsToolTipWidget::setText(const QString& text)
{
    m_textLabel->setText(text);
    forceLayoutUpdate();
}

void QnGraphicsToolTipWidget::setThumbnailVisible(bool visible)
{
    if (m_previewWidget->isHidden() != visible)
        return;

    m_textLabel->setSizePolicy(
        visible ? QSizePolicy::Ignored : QSizePolicy::Preferred,
        QSizePolicy::Preferred);

    m_textLabel->setWordWrapMode(visible
        ? QTextOption::WrapAtWordBoundaryOrAnywhere
        : QTextOption::ManualWrap);

    m_previewWidget->setVisible(visible);
    forceLayoutUpdate();

    updateTailPos();
}

void QnGraphicsToolTipWidget::setImageProvider(QnImageProvider* provider)
{
    if (m_previewWidget->imageProvider() == provider)
        return;

    m_previewWidget->setImageProvider(provider);
    forceLayoutUpdate();
}

QSize QnGraphicsToolTipWidget::maxThumbnailSize() const
{
    return m_maxThumbnailSize;
}

void QnGraphicsToolTipWidget::setMaxThumbnailSize(const QSize& value)
{
    if (m_maxThumbnailSize == value)
        return;

    m_maxThumbnailSize = value;

    // Specify maximum width and height for the widget
    m_previewWidget->setMaximumSize(value);
    forceLayoutUpdate();
}

void QnGraphicsToolTipWidget::updateTailPos()
{
    QRectF rect = this->rect();
    QGraphicsWidget* parent = parentWidget();
    QRectF enclosingRect = parent->geometry();

    // half of the tooltip height in coordinates of enclosing rect
    qreal halfHeight = mapRectToItem(parent, rect).height() / 2;

    qreal parentPos = m_pointTo.y();

    if (parentPos - halfHeight < 0)
        setTailPos(QPointF(qRound(rect.left() - tailLength()), qRound(rect.top() + tailWidth())));
    else if (parentPos + halfHeight > enclosingRect.height())
        setTailPos(QPointF(qRound(rect.left() - tailLength()), qRound(rect.bottom() - tailWidth())));
    else
        setTailPos(QPointF(qRound(rect.left() - tailLength()), qRound((rect.top() + rect.bottom()) / 2)));

    base_type::pointTo(m_pointTo);
}

void QnGraphicsToolTipWidget::pointTo(const QPointF& pos)
{
    m_pointTo = pos;
    base_type::pointTo(pos);
    updateTailPos();
}
