#include "hint_button.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QStylePainter>
#include <QtWidgets/QGroupBox>
#include <QMouseEvent>
#include <QToolTip>

#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/style/nx_style.h>
#include <ui/style/helper.h>
#include <ui/help/help_handler.h>
#include <utils/common/scoped_painter_rollback.h>

namespace {
// Draws a rhombus shape to highlight current widget geometry.
// TODO: #dkargin I want to keep it here for some time, until
// I find a better place for it.
void drawDebugRhombus(QPainter* painter, QRect area)
{
    int cx = area.center().x();
    int cy = area.center().y();
    QPoint poly[] =
    {
        QPoint(area.left(), cy),
        QPoint(cx, area.top() - 1),
        QPoint(area.right(), cy),
        QPoint(cx, area.bottom() - 1)
    };

    QnScopedPainterBrushRollback brushRollback(painter);
    painter->setBrush(QColor(128, 128, 190));
    painter->drawConvexPolygon(poly, 4);
}

} // namespace

namespace nx {
namespace client {
namespace desktop {

HintButton::HintButton(QWidget* parent):
    base_type(parent),
    m_helpTopicId(Qn::Empty_Help)
{
    m_normal = qnSkin->pixmap(lit("buttons/context_info.png"), true);
    m_highlighted = qnSkin->pixmap(lit("buttons/context_info_hovered.png"), true);

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    QSize hintSize = hintMarkSize();
    setFixedSize(hintSize);
    // For hovering stuff
    setMouseTracking(true);
}

HintButton* HintButton::hintThat(QGroupBox* groupBox)
{
    auto result = new HintButton(groupBox);
    groupBox->installEventFilter(result);
    result->updateGeometry(groupBox);
    result->m_parentBox = groupBox;
    return result;
}

bool HintButton::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::Resize && obj == m_parentBox)
        updateGeometry(m_parentBox.data());
    return QWidget::eventFilter(obj, event);
}

// Awful hackery to get access to protected member function
template<class T> struct DenyProtectedStyle: public T
{
    void publicInitStyleOption(QStyleOptionGroupBox* option) const
    {
        this->initStyleOption(option);
    }
};

void HintButton::updateGeometry(QGroupBox* parent)
{
    auto unlockedStyleInit = static_cast<DenyProtectedStyle<QGroupBox>*>(parent);
    QStyleOptionGroupBox box;
    unlockedStyleInit->publicInitStyleOption(&box);

    QRect captionRect = style()->subControlRect(QStyle::CC_GroupBox, &box,
        QStyle::SC_GroupBoxLabel, parent);
    QSize pixmapSize = hintMarkSize();

    // Adjusting own position to land right after the caption ends
    QRect rect;
    rect.setSize(pixmapSize);
    rect.moveCenter(captionRect.center());

    // We manually add some spaces to the caption of group box, to push away its border
    // and provide some space to hint button.
    int offset = parent->isFlat() ? style::Metrics::kHintButtonMargin : style::Metrics::kHintButtonMargin - pixmapSize.width();
    rect.moveLeft(captionRect.right() + offset);
    setGeometry(rect);
}

// Returns prefered size from internal pixmap.
QSize HintButton::hintMarkSize() const
{
    QSize normal = m_normal.size() / m_normal.devicePixelRatioF();
    QSize highlighted = m_highlighted.size() / m_highlighted.devicePixelRatioF();
    return QSize(std::max(normal.width(), highlighted.width()),
        std::max(normal.height(), highlighted.height()));
}

bool HintButton::isClickable() const
{
    return m_helpTopicId != Qn::Empty_Help;
}

void HintButton::showTooltip(bool show)
{
    if (!m_tooltipVisible && show)
    {
        QString text = lit("<p>%1</p>").arg(m_hint);

        auto nxStyle = QnNxStyle::instance();
        NX_ASSERT(nxStyle);

        if (isClickable())
        {
            QColor lineColor = palette().color(QPalette::Text);
            QString colorHex = lineColor.name(QColor::HexRgb);

            text += lit("<i style='color: %1'>%2</i>").arg(colorHex, tr("Click to read more"));
        }

        QRect rc = rect();
        QPoint hintSpawnPos = mapToGlobal(rc.bottomLeft());
        QToolTip::showText(hintSpawnPos, text);
        m_tooltipVisible = true;
    }
    else if (m_tooltipVisible && !show)
    {
        m_tooltipVisible = false;
    }
}

void HintButton::setHint(QString hint)
{
    m_hint = hint;
}

QString HintButton::hint() const
{
    return m_hint;
}

QSize HintButton::sizeHint() const
{
    return m_normal.size();
}

void HintButton::addHintLine(QString line)
{
    if (m_hint.isEmpty())
        m_hint = line;
    else
    {
        m_hint += lit("<br/>");
        m_hint += line;
    }
}

void HintButton::setHelpTopic(int topicId)
{
    m_helpTopicId = topicId;
}

void HintButton::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);

    // I would like to keep it here for some time.
    //drawDebugRhombus(&painter, rect());

    QPixmap& pixmap = m_isHovered ? m_highlighted : m_normal;
    if (!pixmap.isNull())
    {
        // Always center pixmap
        QPointF centeredCorner = rect().center() - pixmap.rect().center() / pixmap.devicePixelRatioF();
        if (!isEnabled())
            painter.setOpacity(0.3);
        painter.drawPixmap(centeredCorner, pixmap);
    }
}

void HintButton::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_helpTopicId != Qn::Empty_Help)
        QnHelpHandler::openHelpTopic(m_helpTopicId);
}

void HintButton::enterEvent(QEvent* event)
{
    if (!isEnabled())
        return;
    if (!m_isHovered)
    {
        m_isHovered = true;
        update();
    }
    showTooltip(true);
}

void HintButton::leaveEvent(QEvent* event)
{
    if (m_isHovered)
    {
        m_isHovered = false;
        update();
    }
    showTooltip(false);
}

} // namespace desktop
} // namespace client
} // namespace nx
