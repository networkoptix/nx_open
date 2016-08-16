
#include "dialog.h"

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
    show(this); /// Calls static member of QnDialog
}

int QnDialog::exec()
{
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
