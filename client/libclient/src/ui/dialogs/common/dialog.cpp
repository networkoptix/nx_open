#include "dialog.h"

#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>

#include <ui/workaround/cancel_drag.h>

QnDialog::QnDialog(QWidget* parent, Qt::WindowFlags flags) :
    base_type(parent, flags),
    m_resizeToContentsMode(0)
{
    cancelDrag(this);
}

QnDialog::~QnDialog()
{
}

void QnDialog::show(QDialog *dialog)
{
    NX_ASSERT(dialog, Q_FUNC_INFO, "Dialog is null");

    if (!dialog)
        return;

    dialog->show();
    cancelDrag(dialog);
}

void QnDialog::show()
{
    Qt::WindowFlags flags = windowFlags();
    if (helpTopic(this) != Qn::Empty_Help)
        flags |= Qt::WindowContextHelpButtonHint;
    else
        flags &= ~Qt::WindowContextHelpButtonHint;
    setWindowFlags(flags);

    show(this); /// Calls static member of QnDialog
}

int QnDialog::exec()
{
    Qt::WindowFlags flags = windowFlags();
    if (helpTopic(this) != Qn::Empty_Help)
        flags |= Qt::WindowContextHelpButtonHint;
    else
        flags &= ~Qt::WindowContextHelpButtonHint;
    setWindowFlags(flags);

    /* We cannot cancel drag via modal dialog, let parent process it. */
    if (parentWidget())
        cancelDrag(parentWidget());

    return base_type::exec();
}

Qt::Orientations QnDialog::resizeToContentsMode() const
{
    return m_resizeToContentsMode;
}

void QnDialog::setResizeToContentsMode(Qt::Orientations mode)
{
    if (m_resizeToContentsMode == mode)
        return;

    m_resizeToContentsMode = mode;
    updateGeometry();
}

bool QnDialog::event(QEvent* event)
{
    bool res = base_type::event(event);

    if (event->type() == QEvent::LayoutRequest)
        afterLayout();

    return res;
}

void QnDialog::afterLayout()
{
    QSize preferred;
    if (m_resizeToContentsMode.testFlag(Qt::Horizontal))
    {
        preferred = sizeHint();
        setFixedWidth(preferred.width());
    }

    if (m_resizeToContentsMode.testFlag(Qt::Vertical))
    {
        if (hasHeightForWidth())
        {
            setFixedHeight(heightForWidth(width()));
        }
        else
        {
            if (!preferred.isValid())
                preferred = sizeHint();
            setFixedHeight(preferred.height());
        }
    }
}
