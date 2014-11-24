#include "non_modal_dialog_constructor.h"

namespace {
    const int extrah = 40;
    const int extraw = 10;
}

void QnShowDialogHelper::show(QWidget* dialog, const QRect &targetGeometry) {
    dialog->showNormal();
    dialog->raise();
    dialog->activateWindow(); // TODO: #Elric show raise activateWindow? Maybe we should also do grabKeyboard, grabMouse? wtf, really?
    if (!targetGeometry.isNull()) {
        dialog->setGeometry(targetGeometry);
    }
    else {
        QPoint targetPoint = calculatePosition(dialog);
        if (!targetPoint.isNull())
            dialog->move(targetPoint);
    }
}

QPoint QnShowDialogHelper::calculatePosition(QWidget* dialog) {
    Q_ASSERT(dialog->parentWidget());
    if (!dialog->parentWidget())
        return QPoint();

    QWidget* w = dialog->parentWidget()->window();
    Q_ASSERT(w);
    if (!w)
        return QPoint();

    int scrn = QApplication::desktop()->screenNumber(w);
    QRect desk = QApplication::desktop()->availableGeometry(scrn);

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
