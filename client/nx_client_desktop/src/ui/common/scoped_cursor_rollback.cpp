#include "scoped_cursor_rollback.h"

#include <QtWidgets/QWidget>

QnScopedCursorRollback::QnScopedCursorRollback(QWidget* widget):
    m_widget(widget),
    m_hadCursor(widget ? widget->testAttribute(Qt::WA_SetCursor) : false),
    m_oldCursor(m_hadCursor ? widget->cursor() : QCursor())
{
}

QnScopedCursorRollback::QnScopedCursorRollback(QWidget* widget, const QCursor& cursor):
    QnScopedCursorRollback(widget)
{
    if (widget)
        widget->setCursor(cursor);
}

QnScopedCursorRollback::~QnScopedCursorRollback()
{
    if (!m_widget)
        return;

    if (m_hadCursor)
        m_widget->setCursor(m_oldCursor);
    else
        m_widget->unsetCursor();
}
