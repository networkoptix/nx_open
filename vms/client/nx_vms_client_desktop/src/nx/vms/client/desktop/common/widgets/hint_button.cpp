#include "hint_button.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QStylePainter>
#include <QtWidgets/QGroupBox>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QToolTip>

#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/style/nx_style.h>
#include <ui/style/helper.h>
#include <ui/help/help_handler.h>
#include <ui/help/help_topic_accessor.h>
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

namespace nx::vms::client::desktop {

HintButton::HintButton(QWidget* parent):
    base_type(parent)
{
    m_normal = qnSkin->pixmap(lit("buttons/context_info.png"), true);
    m_highlighted = qnSkin->pixmap(lit("buttons/context_info_hovered.png"), true);

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    QSize hintSize = hintMarkSize();
    setFixedSize(hintSize);
    // For hovering stuff
    setMouseTracking(true);

    connect(this, &QAbstractButton::clicked, this,
        [this]()
        {
            if (hasHelpTopic())
                QnHelpHandler::openHelpTopic(getHelpTopicId());
        });
}

HintButton* HintButton::hintThat(QGroupBox* groupBox)
{
    auto result = new HintButton(groupBox);
    groupBox->installEventFilter(result);
    result->updateGeometry(groupBox);
    return result;
}

bool HintButton::eventFilter(QObject* obj, QEvent* event)
{
    // We have subscribed exactly to events of QGroupBox instance,
    // so we are quite confident in objects we get here.
    if (event->type() == QEvent::Resize)
        updateGeometry(static_cast<QGroupBox*>(obj));
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
    m_hostBox = parent;
    auto unlockedStyleInit = static_cast<DenyProtectedStyle<QGroupBox>*>(parent);
    QStyleOptionGroupBox box;
    unlockedStyleInit->publicInitStyleOption(&box);

    QRect captionRect = style()->subControlRect(QStyle::CC_GroupBox, &box,
        QStyle::SC_GroupBoxLabel, parent);
    QSize pixmapSize = hintMarkSize();

    // Adjusting own position to land right after the caption ends.
    QRect rect;
    rect.setSize(pixmapSize);
    rect.moveCenter(captionRect.center());

    // We manually add some spaces to the caption of group box, to push away its border
    // and provide some space to hint button.
    int margin = style::Metrics::kHintButtonMargin;
    int offset = parent->isFlat() ? margin : margin - pixmapSize.width();
    rect.moveLeft(captionRect.right() + offset);
    setGeometry(rect);
}

// Returns prefered size from internal pixmap.
QSize HintButton::hintMarkSize() const
{
    return m_normal.size() / m_normal.devicePixelRatioF();
}

int HintButton::getHelpTopicId() const
{
    return helpTopic((QWidget*)this);
}

bool HintButton::hasHelpTopic() const
{
    auto id = getHelpTopicId();
    return id != Qn::Empty_Help;
}

void HintButton::showTooltip(bool show)
{
    if (!m_tooltipVisible && show)
    {
        QString text = lit("<p>%1</p>").arg(m_hint);

        auto nxStyle = QnNxStyle::instance();
        NX_ASSERT(nxStyle);

        if (hasHelpTopic())
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

void HintButton::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);

    // I would like to keep it here for some time.
    //drawDebugRhombus(&painter, rect());

    bool highlight = isDown() ? false : m_isHovered;

    QPixmap& pixmap = highlight ? m_highlighted : m_normal;
    if (!pixmap.isNull())
    {
        // Always center pixmap
        QPointF centeredCorner = rect().center() - pixmap.rect().center() / pixmap.devicePixelRatioF();

        bool disabledState = !isEnabled();
        // We do not show disabled state for the cases we embed this hint into a QGroupBox.
        // This case can be changed in future.
        if (m_hostBox && m_hostBox->isCheckable())
            disabledState = false;

        if (disabledState)
            painter.setOpacity(0.3);
        painter.drawPixmap(centeredCorner, pixmap);
    }
}

void HintButton::enterEvent(QEvent* /*event*/)
{
    bool skipHover = !isEnabled();

    // There is an exception for QGroupBox, when we keep the hint enabled.
    if (m_hostBox && m_hostBox->isCheckable())
        skipHover = false;

    if (skipHover)
        return;

    if (!m_isHovered)
    {
        m_isHovered = true;
        update();
    }
    showTooltip(true);
}

void HintButton::leaveEvent(QEvent* /*event*/)
{
    if (m_isHovered)
    {
        m_isHovered = false;
        update();
    }
    showTooltip(false);
}

} // namespace nx::vms::client::desktop
