// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "hint_button.h"

#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QStyleOptionGroupBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QToolTip>

#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/style/style.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <ui/help/help_handler.h>
#include <ui/help/help_topic_accessor.h>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>

namespace {

static constexpr int kGroupBoxExtraMargin = 2;

QString makeParagraph(const QString& text)
{
    return QString("<p>%1</p>").arg(text);
}

QString makeColoredText(const QString& text, const QColor& color)
{
    return QString("<i style='color: %1'>%2</i>").arg(color.name(QColor::HexRgb), text);
}

} // namespace

namespace nx::vms::client::desktop {

HintButton::HintButton(QWidget* parent):
    base_type(parent),
    m_regularPixmap(qnSkin->pixmap("buttons/context_info.png")),
    m_highlightedPixmap(qnSkin->pixmap("buttons/context_info_hovered.png"))
{
    const auto pixmapSizeScaled = m_regularPixmap.size() / m_regularPixmap.devicePixelRatioF();
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFixedSize(pixmapSizeScaled);

    // For hovering stuff
    setMouseTracking(true);
}

HintButton::HintButton(const QString& hintText, QWidget* parent):
    HintButton(parent)
{
    setHintText(hintText);
}

HintButton* HintButton::createGroupBoxHint(QGroupBox* groupBox)
{
    if (!NX_ASSERT(groupBox, "Valid pointer to the group box expected"))
        return nullptr;

    const auto result = new HintButton(groupBox);
    groupBox->installEventFilter(result);
    result->updateGeometry(groupBox);

    return result;
}

QString HintButton::hintText() const
{
    return m_hintText;
}

void HintButton::setHintText(const QString& hintText)
{
    m_hintText = hintText.trimmed();
}

void HintButton::addHintLine(const QString& hintLine)
{
    m_hintText.append(QChar::LineFeed);
    m_hintText.append(hintLine.trimmed());
}

bool HintButton::eventFilter(QObject* obj, QEvent* event)
{
    // We have subscribed exactly to events of QGroupBox instance,
    // so we are quite confident in objects we get here.
    if (event->type() == QEvent::Resize)
        updateGeometry(static_cast<QGroupBox*>(obj));
    return QWidget::eventFilter(obj, event);
}

// Get access to protected member function
template<class T> struct DenyProtectedStyle: public T
{
    void publicInitStyleOption(QStyleOptionGroupBox* option) const
    {
        this->initStyleOption(option);
    }
};

void HintButton::updateGeometry(QGroupBox* parent)
{
    m_groupBox = parent;

    auto unlockedStyleInit = static_cast<DenyProtectedStyle<QGroupBox>*>(parent);
    QStyleOptionGroupBox groupBoxStyleOption;
    unlockedStyleInit->publicInitStyleOption(&groupBoxStyleOption);

    QRect captionRect = style()->subControlRect(
        QStyle::CC_GroupBox, &groupBoxStyleOption, QStyle::SC_GroupBoxLabel, parent);

    // Adjusting own position to land right after the caption ends.
    QRect newGeometry({}, size());
    newGeometry.moveCenter(captionRect.center());

    // We manually add some spaces to the caption of group box, to push away its border
    // and provide some space to hint button.
    static constexpr auto kMargin = style::Metrics::kHintButtonMargin + kGroupBoxExtraMargin;
    newGeometry.moveLeft(captionRect.right() + kMargin);
    setGeometry(newGeometry);
}

int HintButton::getHelpTopicId() const
{
    return helpTopic(this);
}

bool HintButton::hasHelpTopic() const
{
    return getHelpTopicId() != Qn::Empty_Help;
}

void HintButton::showTooltip(bool show)
{
    if (!m_tooltipVisible && show)
    {
        const auto hintLines = m_hintText.split(QChar::LineFeed);

        QStringList hintParagraphs;
        for (const auto& hintLine: hintLines)
            hintParagraphs.push_back(makeParagraph(hintLine));

        if (hasHelpTopic())
        {
            hintParagraphs.push_back(makeParagraph(
                makeColoredText(tr("Click on the icon to read more"),
                    colorTheme()->color("light16"))));
        }

        QPoint hintSpawnPos = mapToGlobal(rect().bottomLeft());
        QToolTip::showText(hintSpawnPos, hintParagraphs.join(""));
        m_tooltipVisible = true;
    }
    else if (m_tooltipVisible && !show)
    {
        m_tooltipVisible = false;
    }
}

bool HintButton::event(QEvent* event)
{
    // clicked() signal will not be emitted and mouseReleaseEvent() will not be called if
    // hint button is disabled due to not checked parent QGroupBox, so we catch click this way.
    if (event->type() == QEvent::MouseButtonRelease && hasHelpTopic())
        QnHelpHandler::openHelpTopic(getHelpTopicId());

    return base_type::event(event);
}

void HintButton::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);

    const bool highlight = !isDown() && m_isHovered;
    const auto& pixmap = highlight ? m_highlightedPixmap : m_regularPixmap;

    if (!pixmap.isNull())
    {
        // Always center pixmap
        QPointF centeredCorner = rect().center() - pixmap.rect().center() / pixmap.devicePixelRatioF();

        bool disabledState = !isEnabled();
        // We do not show disabled state for the cases we embed this hint into a QGroupBox.
        // This case can be changed in future.
        if (m_groupBox && m_groupBox->isCheckable())
            disabledState = false;

        if (disabledState)
            painter.setOpacity(0.3);

        painter.drawPixmap(centeredCorner, pixmap);
    }
}

void HintButton::enterEvent(QEnterEvent* /*event*/)
{
    bool skipHover = !isEnabled();

    // There is an exception for QGroupBox, when we keep the hint enabled.
    if (m_groupBox && m_groupBox->isCheckable())
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
