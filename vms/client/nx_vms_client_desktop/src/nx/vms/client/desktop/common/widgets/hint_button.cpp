// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "hint_button.h"

#include <QtCore/QSize>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QStyleOptionGroupBox>
#include <QtWidgets/QToolTip>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/help/help_handler.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/style.h>

namespace {

// Extra margin for a hint button attached to group box or header view.
static constexpr int kExtraMargin = 2;

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

const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions HintButton::kIconSubstitutions = {
    {QIcon::Normal, {.primary = "dark14"}},
    {QIcon::Active, {.primary = "dark15"}}};

HintButton::HintButton(QWidget* parent):
    base_type(parent),
    m_regularPixmap(qnSkin->icon("buttons/context_hint_16.svg", kIconSubstitutions)
        .pixmap(QSize(16,16))),
    m_highlightedPixmap(qnSkin->icon("buttons/context_hint_16.svg", kIconSubstitutions)
        .pixmap(QSize(16,16), QIcon::Mode::Active))
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

HintButton* HintButton::createHeaderViewHint(QHeaderView* headerView, int section)
{
    if (!NX_ASSERT(headerView, "Valid pointer to the header view expected"))
        return nullptr;

    if (!NX_ASSERT(headerView->orientation() == Qt::Horizontal, "Horizontal header view expected"))
        return nullptr;

    const auto model = headerView->model();
    if (!NX_ASSERT(model || (section < 0) || section >= model->columnCount()))
        return nullptr;

    const auto hintButton = new HintButton(headerView);
    hintButton->m_headerViewSection = section;

    connect(headerView, &QHeaderView::geometriesChanged, hintButton,
        [hintButton, headerView]{ hintButton->updateGeometry(headerView); });

    connect(headerView, &QHeaderView::sectionMoved, hintButton,
        [hintButton, headerView]{ hintButton->updateGeometry(headerView); });

    return hintButton;
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

bool HintButton::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::Resize)
    {
        if (auto groupBox = qobject_cast<QGroupBox*>(object))
            updateGeometry(groupBox);
    }

    return QWidget::eventFilter(object, event);
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
    static constexpr auto kMargin = style::Metrics::kHintButtonMargin + kExtraMargin;
    newGeometry.moveLeft(captionRect.right() + kMargin);
    setGeometry(newGeometry);
}

void HintButton::updateGeometry(QHeaderView* headerView)
{
    static constexpr auto kMargin = style::Metrics::kHintButtonMargin + kExtraMargin;

    const QRect sectionRect(
        headerView->sectionViewportPosition(*m_headerViewSection), 0,
        headerView->sectionSize(*m_headerViewSection), headerView->height());

    const auto model = headerView->model();
    const auto textData = model->headerData(*m_headerViewSection, Qt::Horizontal);
    const auto textAlignmentData =
        model->headerData(*m_headerViewSection, Qt::Horizontal, Qt::TextAlignmentRole);

    QStyleOptionHeader sectionStyleOption;
    sectionStyleOption.initFrom(headerView);
    sectionStyleOption.text = textData.toString();
    sectionStyleOption.section = *m_headerViewSection;
    sectionStyleOption.textAlignment = textAlignmentData.isValid()
        ? Qt::Alignment(textAlignmentData.toInt())
        : headerView->defaultAlignment();
    sectionStyleOption.rect = sectionRect;

    const auto sectionTextRect =
        style()->subElementRect(QStyle::SE_HeaderLabel, &sectionStyleOption, headerView);

    QRect updatedGeometry({}, size());
    updatedGeometry.moveCenter(headerView->rect().center());
    updatedGeometry.moveLeft(sectionTextRect.right() + kMargin);

    setGeometry(updatedGeometry);
}

int HintButton::getHelpTopicId() const
{
    return helpTopic(this);
}

bool HintButton::hasHelpTopic() const
{
    return getHelpTopicId() != HelpTopic::Id::Empty;
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
                    core::colorTheme()->color("light16"))));
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
        HelpHandler::openHelpTopic(getHelpTopicId());

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
