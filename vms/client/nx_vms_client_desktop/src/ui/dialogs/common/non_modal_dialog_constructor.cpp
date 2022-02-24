// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "non_modal_dialog_constructor.h"

#include <QtCore/QEvent>
#include <QtGui/QWindow>
#include <QtGui/QScreen>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

#include <nx/utils/log/assert.h>

namespace {
    const int extrah = 40;
    const int extraw = 10;
}

void QnShowDialogHelper::show(QWidget* dialog, const QRect &targetGeometry) {
    dialog->showNormal();
    dialog->raise();
    dialog->activateWindow(); // TODO: #sivanov Triple call looks like overkill.
    if (!targetGeometry.isNull()) {
        dialog->setGeometry(targetGeometry);
    }
    else {
        QPoint targetPoint = calculatePosition(dialog);
        if (!targetPoint.isNull())
            dialog->move(targetPoint);
    }

    // If dialog's parent changed screens while the dialog was hidden,
    // the dialog appears not painted when shown next time. This line is a workaround.
    dialog->windowHandle()->requestUpdate();
}

QPoint QnShowDialogHelper::calculatePosition(QWidget* dialog) {
    NX_ASSERT(dialog->parentWidget());
    if (!dialog->parentWidget())
        return QPoint();

    QWidget* w = dialog->parentWidget()->window();
    NX_ASSERT(w);
    if (!w)
        return QPoint();

    const QRect desk = w->screen()->availableGeometry();

    // Use pos() if the widget is embedded into a native window
    QPoint pp;
    if (w->windowHandle() && w->windowHandle()->property("_q_embedded_native_parent_handle").value<WId>())
        pp = w->pos();
    else
        pp = w->mapToGlobal(QPoint(0,0));

    QPoint p = QPoint(pp.x() + w->width()/2, pp.y() + w->height()/ 2);

    // p = origin of this
    p = QPoint(p.x() - dialog->width() / 2 - extraw, p.y() - dialog->height() / 2 - extrah);

    if (p.x() + extraw + dialog->width() > desk.x() + desk.width())
        p.setX(desk.x() + desk.width() - dialog->width() - extraw);
    if (p.x() < desk.x())
        p.setX(desk.x());

    if (p.y() + extrah + dialog->height() > desk.y() + desk.height())
        p.setY(desk.y() + desk.height() - dialog->height() - extrah);
    if (p.y() < desk.y())
        p.setY(desk.y());

    return p;
}

void QnShowDialogHelper::showNonModalDialog(QWidget* dialog, const QRect &targetGeometry,
    bool focus)
{
    QWidgetList modalWidgets;

    /* Checking all top-level widgets. */
    for (auto tlw: qApp->topLevelWidgets())
    {
        if (!tlw->isHidden() && tlw->isModal())
            modalWidgets << tlw;
    }

    /* Setup dialog to show after all modal windows are closed. */
    if (!modalWidgets.isEmpty())
    {
        auto helper = new QnDelayedShowHelper(dialog, targetGeometry, modalWidgets.size(), dialog);
        for (auto modalWidget: modalWidgets)
            modalWidget->installEventFilter(helper);
        return;
    }

    if (dialog->isVisible() && !focus)
        dialog->raise();
    else
        show(dialog, targetGeometry);
}

QnDelayedShowHelper::QnDelayedShowHelper(QWidget* targetWidget, const QRect &targetGeometry, int sourceCount, QObject *parent):
    QObject(parent),
    m_targetWidget(targetWidget),
    m_targetGeometry(targetGeometry),
    m_sourceCount(sourceCount)
{

}

bool QnDelayedShowHelper::eventFilter(QObject *watched, QEvent *event)
{
    if (m_targetWidget && event->type() == QEvent::Hide)
    {
        watched->removeEventFilter(this);   //avoid double call
        m_sourceCount--;

        if (m_sourceCount <= 0)
        {
            show(m_targetWidget.data(), m_targetGeometry);
            deleteLater();
        }

    }

    return QObject::eventFilter(watched, event);
}
