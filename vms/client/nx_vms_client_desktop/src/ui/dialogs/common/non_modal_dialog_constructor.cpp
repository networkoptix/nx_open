// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "non_modal_dialog_constructor.h"

#include <QtCore/QEvent>
#include <QtGui/QScreen>
#include <QtGui/QWindow>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>

namespace {

QString widgetInfo(const QObject* object)
{
    QStringList names;
    if (auto name = object->objectName(); !name.isEmpty())
        names.push_back(NX_FMT("Object name: %1", name));

    if (auto widget = dynamic_cast<const QWidget*>(object))
    {
        if (auto title = widget->windowTitle(); !title.isEmpty())
            names.push_back(NX_FMT("Window title: %1", title));

        names.push_back(NX_FMT("Modality: %1, visible: %2, hidden: %3",
            widget->windowModality(), widget->isVisible(), widget->isHidden()));
    }

    return names.join(", ");
}

QString widgetsInfo(const QWidgetList& widgets)
{
    QStringList names;
    for (auto widget: widgets)
        names.push_back(widgetInfo(widget));
    names.removeAll({});

    return names.join("\n");
}

} // namespace

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

    constexpr auto extrah = 40;
    constexpr auto extraw = 10;

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

void QnShowDialogHelper::showNonModalDialog(
    QWidget* dialog, const QRect& targetGeometry, bool focus)
{
    QWidgetList modalWidgets;

    /* Checking all top-level widgets. */
    for (auto tlw: qApp->topLevelWidgets())
    {
        if (!tlw->isHidden() && tlw->isModal())
            modalWidgets << tlw;
    }

    NX_DEBUG(NX_SCOPE_TAG, "Show non-modal dialog: %1, modal widgets: %2",
        dialog->windowTitle(), modalWidgets.size());

    NX_ASSERT(modalWidgets.empty(),
        "Non-modal dialog: %1 is shown while %2 modal dialogs are visible:\n%3",
        dialog->windowTitle(), modalWidgets.size(), widgetsInfo(modalWidgets));

    if (dialog->isVisible() && !focus)
        dialog->raise();
    else
        show(dialog, targetGeometry);
}
