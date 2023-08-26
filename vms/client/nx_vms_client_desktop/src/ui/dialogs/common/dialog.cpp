// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "dialog.h"

#include <QtCore/QEvent>
#include <QtWidgets/QLayout>

#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <ui/workaround/cancel_drag.h>

#if defined(Q_OS_WIN)
    #include <nx/vms/client/desktop/common/widgets/dialog_title_bar_widget.h>
    #include <nx/vms/client/desktop/common/widgets/emulated_non_client_area.h>
    #include <nx/vms/client/desktop/ini.h>
    #include <nx/vms/client/desktop/platforms/windows/system_non_client_area_remover_win.h>
#endif // defined(Q_OS_WIN)

using namespace nx::vms::client::desktop;

QnDialog::QnDialog(QWidget* parent, Qt::WindowFlags flags):
    base_type(parent, flags),
    m_safeMinimumSize(nx::style::Metrics::kMinimumDialogSize)
{
    cancelDrag(this);
#if defined(Q_OS_WIN)
    if (ini().overrideDialogFramesWIN)
    {
        EmulatedNonClientArea::create(this, new DialogTitleBarWidget());
        SystemNonClientAreaRemover::instance().apply(this);
    }
#endif // defined(Q_OS_WIN)
}

QnDialog::~QnDialog()
{
}

void QnDialog::show()
{
    fixWindowFlags();
    QDialog::show();
    cancelDrag(this);
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
    if (helpTopic(this) != HelpTopic::Id::Empty)
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
