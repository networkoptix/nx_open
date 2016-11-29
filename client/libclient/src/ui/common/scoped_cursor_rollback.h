#pragma once

#include <QtCore/QPointer>
#include <QtGui/QCursor>

class QWidget;
class QnScopedCursorRollback final
{
public:
    explicit QnScopedCursorRollback(QWidget* widget);
    QnScopedCursorRollback(QWidget* widget, const QCursor& cursor);

    ~QnScopedCursorRollback();

private:
    const QPointer<QWidget> m_widget;
    const bool m_hadCursor;
    const QCursor m_oldCursor;
};
