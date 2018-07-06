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

namespace {
    // Offset between header and hint button
    const int kHeaderHintOffset = 4;
}

namespace nx {
namespace client {
namespace desktop {

HintButton::HintButton(QWidget* parent)
    :base_type(parent)
{
    m_helpTopicId = Qn::Empty_Help;
    m_normal = qnSkin->pixmap(lit("buttons/context_info.png"), true);
    m_highlighted = qnSkin->pixmap(lit("buttons/context_info_hovered.png"), true);

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

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
    {
        QResizeEvent *resizeEvent = static_cast<QResizeEvent*>(event);
        updateGeometry(m_parentBox.data());
    }
    return QWidget::eventFilter(obj, event);
}

// Awful hackery to get access to protected member function
template<class T> struct DenyProtectedStyle: public T
{
    void publicInitStyleOption(QStyleOptionGroupBox *option) const
    {
        this->initStyleOption(option);
    }
};

void HintButton::updateGeometry(QGroupBox* parent)
{
    auto* unlockedStyleInit = static_cast<DenyProtectedStyle<QGroupBox>*>(parent);
    QStyleOptionGroupBox box;
    unlockedStyleInit->publicInitStyleOption(&box);
    int captionWidth = box.fontMetrics.width(parent->title());

    // #dkargin: I just like this magnificent line and want to keep it here for some time.
    // QRect contentsRect = style()->subControlRect(QStyle::CC_GroupBox, &box, QStyle::SC_GroupBoxContents, q);

    // There is a checkbox, so we should add its width as well
    if (parent->isCheckable())
    {
        captionWidth += style()->pixelMetric(QStyle::PM_IndicatorWidth);
        captionWidth += style()->pixelMetric(QStyle::PM_CheckBoxLabelSpacing);
    }

    // Adjusting own position to land right after the caption ends
    QRect rect;
    rect.setSize(size());
    rect.moveLeft(captionWidth + kHeaderHintOffset);
    setGeometry(rect);
}

bool HintButton::isClickable() const
{
    return m_helpTopicId != Qn::Empty_Help;
}

void HintButton::showTooltip(bool show)
{
    if (!m_tooltipVisible && show)
    {
        QRect rc = rect();
        QPoint hintSpawnPos = mapToGlobal(rc.bottomLeft());
        QString text = lit("<p>%1</p>").arg(m_hint);

        auto nxStyle = QnNxStyle::instance();
        NX_ASSERT(nxStyle);

        if (isClickable())
        {
            QColor lineColor = palette().color(QPalette::Text);
            QString colorHex = lineColor.name(QColor::HexRgb);

            text += lit("<br/><i style='color: %1'>%2</i>").arg(colorHex, tr("Click to read more"));
        }

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

    QPixmap& pixmap = m_isHovered ? m_highlighted : m_normal;
    if (!pixmap.isNull())
    {
        // Always center pixmap
        QPointF centeredCorner = rect().center() - pixmap.rect().center();
        if (!isEnabled())
            painter.setOpacity(0.3);
        painter.drawPixmap(centeredCorner*0.5, pixmap);
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
