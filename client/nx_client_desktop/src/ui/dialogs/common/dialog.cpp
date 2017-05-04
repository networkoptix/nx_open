#include "dialog.h"

#include <QtCore/QEvent>

#include <QtWidgets/QLayout>

#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/style/helper.h>
#include <ui/workaround/cancel_drag.h>

QnDialog::QnDialog(QWidget* parent, Qt::WindowFlags flags):
    base_type(parent, flags),
    m_resizeToContentsMode(0),
    m_safeMinimumSize(style::Metrics::kMinimumDialogSize)
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
    fixWindowFlags();
    show(this); /// Calls static member of QnDialog
}

int QnDialog::exec()
{
    fixWindowFlags();

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

    switch (event->type())
    {
        case QEvent::LayoutRequest:
            afterLayout();
            break;

        case QEvent::Show:
            if (layout())
                layout()->activate();
            break;

        default:
            break;
    }

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

void QnDialog::fixWindowFlags()
{
    auto flags = windowFlags();
    if (helpTopic(this) != Qn::Empty_Help)
        flags |= Qt::WindowContextHelpButtonHint;
    else
        flags &= ~Qt::WindowContextHelpButtonHint;
    setWindowFlags(flags);
}

QSize QnDialog::sizeHint() const
{
    return base_type::sizeHint().expandedTo(minimumSizeHint());
}

QSize QnDialog::minimumSizeHint() const
{
    return base_type::minimumSizeHint().expandedTo(m_safeMinimumSize);
}

void QnDialog::setSafeMinimumWidth(int width)
{
    setSafeMinimumSize({ width, safeMinimumHeight() });
}

int QnDialog::safeMinimumWidth() const
{
    return m_safeMinimumSize.width();
}

void QnDialog::setSafeMinimumHeight(int height)
{
    setSafeMinimumSize({ safeMinimumWidth(), height });
}

int QnDialog::safeMinimumHeight() const
{
    return m_safeMinimumSize.height();
}

void QnDialog::setSafeMinimumSize(const QSize& size)
{
    if (size == m_safeMinimumSize)
        return;

    m_safeMinimumSize = size;
    updateGeometry();
}

QSize QnDialog::safeMinimumSize() const
{
    return m_safeMinimumSize;
}
