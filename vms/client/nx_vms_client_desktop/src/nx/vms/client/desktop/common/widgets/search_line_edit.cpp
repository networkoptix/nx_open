// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "search_line_edit.h"

#include <QtWidgets/QCompleter>
#include <QtWidgets/QLineEdit>
#include <QtGui/QAction>

#include <ui/common/palette.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <utils/common/delayed.h>

#include <nx/utils/pending_operation.h>

namespace nx::vms::client::desktop {

SearchLineEdit::SearchLineEdit(QWidget* parent):
    LineEdit(parent),
    m_emitTextChanged(new utils::PendingOperation(
        [this]() { emit textChanged(m_lineEdit->text()); }, 0, this))
{
    m_lineEdit->setPlaceholderText(tr("Search"));
    m_glassIcon = m_lineEdit->addAction(qnSkin->icon("theme/input_search.png"),
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
