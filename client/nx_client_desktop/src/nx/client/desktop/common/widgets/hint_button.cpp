#include "hint_button.h"

#include <QtWidgets/qmenu.h>

#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStylePainter>
#include <QtWidgets/QVBoxLayout>
#include <QMouseEvent>

#include <QToolTip>

#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/style/nx_style.h>
#include <ui/style/helper.h>

#include <ui/help/help_handler.h>

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
            QnGenericPalette palette = nxStyle->genericPalette();
            QnPaletteColor lineColor = palette.color(lit("light"), 16);
            QString colorHex = lineColor.color().name(QColor::HexRgb);

            text += lit("<p><i style='color: %1'>%2</i></p>").arg(colorHex, tr("Click to read more"));
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
    qDebug() << "HintButton::enterEvent";
    if (!m_isHovered)
    {
        m_isHovered = true;
        update();
    }
    showTooltip(true);
}

void HintButton::leaveEvent(QEvent* event)
{
    qDebug() << "HintButton::leaveEvent";
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
