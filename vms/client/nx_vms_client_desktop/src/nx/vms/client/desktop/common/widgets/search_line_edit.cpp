// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "search_line_edit.h"

#include <QtGui/QAction>
#include <QtWidgets/QCompleter>
#include <QtWidgets/QLineEdit>

#include <nx/utils/pending_operation.h>
#include <nx/vms/client/core/skin/skin.h>
#include <ui/common/palette.h>
#include <utils/common/delayed.h>

namespace {

static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kSearchSubstitutions =
    {{QnIcon::Normal, {.primary = "light16"}}};

NX_DECLARE_COLORIZED_ICON(kSearchIcon, "16x16/Outline/search.svg", kSearchSubstitutions);

}  // namespace

namespace nx::vms::client::desktop {

SearchLineEdit::SearchLineEdit(QWidget* parent):
    LineEdit(parent),
    m_emitTextChanged(new utils::PendingOperation(
        [this]() { emit textChanged(m_lineEdit->text()); }, 0, this))
{
    m_lineEdit->setPlaceholderText(tr("Search"));
    m_glassIcon = m_lineEdit->addAction(qnSkin->icon(kSearchIcon),
        QLineEdit::LeadingPosition);
    m_lineEdit->setClearButtonEnabled(true);

    m_emitTextChanged->setFlags(utils::PendingOperation::FireOnlyWhenIdle);

    connect(m_lineEdit, &QLineEdit::returnPressed, this, &SearchLineEdit::enterKeyPressed);

    connect(m_lineEdit, &QLineEdit::textChanged,
        m_emitTextChanged.data(), &utils::PendingOperation::requestOperation);

    QSizePolicy policy = sizePolicy();
    setSizePolicy(QSizePolicy::Preferred, policy.verticalPolicy());

    // Sets empty frame and invalidates geometry of control. Should be called outside constructor
    // to prevent wrong layout size recalculations.
    executeLater([this](){ m_lineEdit->setFrame(false); }, this);
}

SearchLineEdit::~SearchLineEdit()
{
}

int SearchLineEdit::textChangedSignalFilterMs() const
{
    return m_emitTextChanged->intervalMs();
}

void SearchLineEdit::setTextChangedSignalFilterMs(int filterMs)
{
    m_emitTextChanged->setIntervalMs(filterMs);
}

void SearchLineEdit::setGlassVisible(bool visible)
{
    m_glassIcon->setVisible(visible);
}

} // namespace nx::vms::client::desktop
