// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "search_line_edit.h"

#include <QtGui/QAction>
#include <QtWidgets/QCompleter>
#include <QtWidgets/QLineEdit>

#include <nx/utils/pending_operation.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <utils/common/delayed.h>

namespace {
static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kSearchSubstitutions = {
    {QnIcon::Disabled, {.primary = "light16"}}, {QnIcon::Normal, {.primary = "light1"}}};
const QSize kIconSize = {16, 16};
const int kMinimumHeight = 28;
NX_DECLARE_COLORIZED_ICON(kSearchIcon, "16x16/Outline/search.svg", kSearchSubstitutions);

QnIcon::Mode calculateIconMode(const QLineEdit* lineEdit)
{
    return lineEdit->text().isEmpty() ? QnIcon::Disabled : QnIcon::Normal;
}

} // namespace

namespace nx::vms::client::desktop {

SearchLineEdit::SearchLineEdit(QWidget* parent):
    LineEdit(parent),
    m_emitTextChanged(new utils::PendingOperation(
        [this]() { emit textChanged(m_lineEdit->text()); }, 0, this))
{
    m_lineEdit->setPlaceholderText(tr("Search"));
    m_currentIconMode = calculateIconMode(m_lineEdit);
    m_searchAction =
        m_lineEdit->addAction(qnSkin->icon(kSearchIcon).pixmap(kIconSize, m_currentIconMode),
        QLineEdit::LeadingPosition);

    setColors();
    m_lineEdit->setMinimumHeight(kMinimumHeight);

    m_lineEdit->setClearButtonEnabled(true);

    m_emitTextChanged->setFlags(utils::PendingOperation::FireOnlyWhenIdle);

    connect(m_lineEdit, &QLineEdit::returnPressed, this, &SearchLineEdit::enterKeyPressed);

    connect(m_lineEdit, &QLineEdit::textChanged,
        m_emitTextChanged.data(), &utils::PendingOperation::requestOperation);

    connect(m_lineEdit, &QLineEdit::textChanged, this,
        [this]()
        {
            const auto expectedMode = calculateIconMode(m_lineEdit);
            if (expectedMode == m_currentIconMode)
                return;

            m_currentIconMode = expectedMode;
            m_searchAction->setIcon(qnSkin->icon(kSearchIcon).pixmap(kIconSize, m_currentIconMode));
        });

    QSizePolicy policy = sizePolicy();
    setSizePolicy(QSizePolicy::Preferred, policy.verticalPolicy());

    // Sets empty frame and invalidates geometry of control. Should be called outside constructor
    // to prevent wrong layout size recalculations.
    executeLater([this](){ m_lineEdit->setFrame(false); }, this);
}

SearchLineEdit::~SearchLineEdit()
{
}

void SearchLineEdit::setDarkMode(bool darkMode)
{
    m_darkMode = darkMode;
    setColors();
}

void SearchLineEdit::setColors()
{
    using namespace nx::vms::client::core;
    const auto backgroundColor = colorTheme()->color(m_darkMode ? "dark3" : "dark5");
    const auto borderColor = colorTheme()->color(m_darkMode ? "dark7" : "dark4");

    m_lineEdit->setStyleSheet(nx::format(
        "QLineEdit { padding-left: 0px; background-color: %1; border: 1px solid %2; "
        "border-radius: 2px; } "
        "QLineEdit:hover { background-color: %3; border: 1px solid %4; } "
        "QLineEdit:focus { background-color: %5; border: 1px solid %6; }",
        backgroundColor.name(),
        borderColor.name(),
        colorTheme()->lighter(backgroundColor, 1).name(),
        m_darkMode ? borderColor.name() : colorTheme()->lighter(borderColor, 1).name(),
        m_darkMode ? backgroundColor.name() : colorTheme()->darker(backgroundColor, 1).name(),
        m_darkMode ? borderColor.name() : colorTheme()->darker(borderColor, 2).name()));
}

int SearchLineEdit::textChangedSignalFilterMs() const
{
    return m_emitTextChanged->intervalMs();
}

void SearchLineEdit::setTextChangedSignalFilterMs(int filterMs)
{
    m_emitTextChanged->setIntervalMs(filterMs);
}
} // namespace nx::vms::client::desktop
