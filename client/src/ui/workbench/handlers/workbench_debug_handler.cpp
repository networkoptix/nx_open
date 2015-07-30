#include "workbench_debug_handler.h"

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <core/resource_management/resource_pool.h>

#include <ui/actions/action.h>
#include <ui/actions/action_manager.h>
#include <ui/widgets/palette_widget.h>
#include <ui/dialogs/resource_list_dialog.h>
#include <ui/workaround/qtbug_workaround.h>

// -------------------------------------------------------------------------- //
// QnDebugControlDialog
// -------------------------------------------------------------------------- //
class QnDebugControlDialog: public QDialog, public QnWorkbenchContextAware {
    typedef QDialog base_type;

public:
    QnDebugControlDialog(QWidget *parent = NULL):
        base_type(parent),
        QnWorkbenchContextAware(parent)
    {
        QVBoxLayout *layout = new QVBoxLayout();
        layout->addWidget(newActionButton(Qn::DebugDecrementCounterAction));
        layout->addWidget(newActionButton(Qn::DebugIncrementCounterAction));
        layout->addWidget(newActionButton(Qn::DebugShowResourcePoolAction));
        setLayout(layout);
    }

private:
    QToolButton *newActionButton(Qn::ActionId actionId, QWidget *parent = NULL) {
        QToolButton *button = new QToolButton(parent);
        button->setDefaultAction(menu()->action(actionId));
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        return button;
    }
};


// -------------------------------------------------------------------------- //
// QnWorkbenchDebugHandler
// -------------------------------------------------------------------------- //
QnWorkbenchDebugHandler::QnWorkbenchDebugHandler(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(action(Qn::DebugControlPanelAction),                &QAction::triggered,    this,   &QnWorkbenchDebugHandler::at_debugControlPanelAction_triggered);
    connect(action(Qn::DebugIncrementCounterAction),            &QAction::triggered,    this,   &QnWorkbenchDebugHandler::at_debugIncrementCounterAction_triggered);
    connect(action(Qn::DebugDecrementCounterAction),            &QAction::triggered,    this,   &QnWorkbenchDebugHandler::at_debugDecrementCounterAction_triggered);
    connect(action(Qn::DebugShowResourcePoolAction),            &QAction::triggered,    this,   &QnWorkbenchDebugHandler::at_debugShowResourcePoolAction_triggered);
}

void QnWorkbenchDebugHandler::at_debugControlPanelAction_triggered() {
    QScopedPointer<QnDebugControlDialog> dialog(new QnDebugControlDialog(mainWindow()));
    dialog->exec();
}

void QnWorkbenchDebugHandler::at_debugIncrementCounterAction_triggered() {
    qnRuntime->setDebugCounter(qnRuntime->debugCounter() + 1);
    qDebug() << qnRuntime->debugCounter();

    auto showPalette = [this] {
        QnPaletteWidget *w = new QnPaletteWidget();
        w->setPalette(qApp->palette());
        w->show();
    };

    // the palette widget code maight still be required
    Q_UNUSED(showPalette);
}

void QnWorkbenchDebugHandler::at_debugDecrementCounterAction_triggered() {
    qnRuntime->setDebugCounter(qnRuntime->debugCounter() - 1);
    qDebug() << qnRuntime->debugCounter();
}

void QnWorkbenchDebugHandler::at_debugShowResourcePoolAction_triggered() {
    QScopedPointer<QnResourceListDialog> dialog(new QnResourceListDialog(mainWindow()));
    dialog->setResources(resourcePool()->getResources());
    dialog->exec();
}
