#pragma once

#include <QtCore/QPointer>
#include <QtGui/QCursor>

class QWidget;
class QnScopedCursorRollback
{
public:
    QnScopedCursorRollback(QWidget* widget);
    QnScopedCursorRollback(QWidget* widget, const QCursor& cursor);

    ~QnScopedCursorRollback();

private:
    QPointer<QWidget> m_widget;
    bool m_hadCursor;
    QCursor m_oldCursor;
};
