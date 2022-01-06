// Modified by Network Optix, Inc. Original copyright notice follows.

/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the demonstration applications of the Qt Toolkit.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
****************************************************************************/

#include "line_edit.h"

#include <QtGui/QFocusEvent>
#include <QtWidgets/QCompleter>
#include <QtWidgets/QLineEdit>
#include <QtGui/QAction>

LineEdit::LineEdit(QWidget* parent):
    QWidget(parent),
    m_lineEdit(new QLineEdit(this))
{
    setFocusPolicy(m_lineEdit->focusPolicy());
    setAttribute(Qt::WA_InputMethodEnabled);
    setSizePolicy(m_lineEdit->sizePolicy());
    setBackgroundRole(m_lineEdit->backgroundRole());
    setMouseTracking(true);
    setAcceptDrops(true);
    setAttribute(Qt::WA_MacShowFocusRect, true);
    QPalette p = m_lineEdit->palette();
    setPalette(p);

    // line edit
    m_lineEdit->setFocusProxy(this);
    m_lineEdit->setAttribute(Qt::WA_MacShowFocusRect, false);
    QPalette clearPalette = m_lineEdit->palette();
    clearPalette.setBrush(QPalette::Base, QBrush(Qt::transparent));
    m_lineEdit->setPalette(clearPalette);
}

LineEdit::~LineEdit()
{
}

void LineEdit::initStyleOption(QStyleOptionFrame* option) const
{
    option->initFrom(this);
    option->rect = contentsRect();
    option->lineWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth, option, this);
    option->midLineWidth = 0;
    option->state |= QStyle::State_Sunken;
    if (m_lineEdit->isReadOnly())
        option->state |= QStyle::State_ReadOnly;
#ifdef QT_KEYPAD_NAVIGATION
    if (hasEditFocus())
        option->state |= QStyle::State_HasEditFocus;
#endif
    option->features = QStyleOptionFrame::None;
}

QSize LineEdit::sizeHint() const
{
    return m_lineEdit->sizeHint();
}

QString LineEdit::text() const
{
    return m_lineEdit->text();
}

void LineEdit::setText(const QString& value)
{
    m_lineEdit->setText(value);
}

void LineEdit::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    m_lineEdit->setGeometry(rect());
}

void LineEdit::focusInEvent(QFocusEvent* event)
{
    m_lineEdit->event(event);
    m_lineEdit->selectAll();
    QWidget::focusInEvent(event);
}

void LineEdit::focusOutEvent(QFocusEvent* event)
{
    m_lineEdit->event(event);

    if (m_lineEdit->completer())
    {
        connect(m_lineEdit->completer(), SIGNAL(activated(QString)),
            m_lineEdit, SLOT(setText(QString)));
        connect(m_lineEdit->completer(), SIGNAL(highlighted(QString)),
            m_lineEdit, SLOT(_q_completionHighlighted(QString)));
    }
    QWidget::focusOutEvent(event);
}

void LineEdit::keyPressEvent(QKeyEvent* event)
{
    m_lineEdit->event(event);
    if ((event->key() == Qt::Key_Enter) || (event->key() == Qt::Key_Return))
        event->accept();
}

void LineEdit::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::EnabledChange)
        emit enabledChanged();
}

bool LineEdit::event(QEvent* event)
{
    if (event->type() == QEvent::ShortcutOverride)
    {
        QKeyEvent * const keyEvent = static_cast<QKeyEvent *>(event);
        const bool result = m_lineEdit->event(event);
        if (keyEvent->key() == Qt::Key_Escape)
        {
            emit escKeyPressed();
            keyEvent->accept();
        }
        return result;
    }

    return QWidget::event(event);
}

QVariant LineEdit::inputMethodQuery(Qt::InputMethodQuery property) const
{
    return m_lineEdit->inputMethodQuery(property);
}

void LineEdit::clear()
{
    m_lineEdit->clear();
}

void LineEdit::inputMethodEvent(QInputMethodEvent* event)
{
    m_lineEdit->event(event);
}
