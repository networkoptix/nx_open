#include "workbench_debug_handler.h"

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <core/resource_management/resource_pool.h>

#include <ui/actions/action.h>
#include <ui/actions/action_manager.h>
#include <ui/widgets/palette_widget.h>
#include <ui/dialogs/common/dialog.h>
#include <ui/dialogs/resource_list_dialog.h>
#include <ui/workaround/qtbug_workaround.h>

// -------------------------------------------------------------------------- //
// QnDebugControlDialog
// -------------------------------------------------------------------------- //
class QnDebugControlDialog: public QnDialog, public QnWorkbenchContextAware {
    typedef QnDialog base_type;

public:
    QnDebugControlDialog(QWidget *parent = NULL):
        base_type(parent),
        QnWorkbenchContextAware(parent)
    {
        QVBoxLayout *layout = new QVBoxLayout();
        layout->addWidget(newActionButton(QnActions::DebugDecrementCounterAction));
        layout->addWidget(newActionButton(QnActions::DebugIncrementCounterAction));
        layout->addWidget(newActionButton(QnActions::DebugShowResourcePoolAction));
        setLayout(layout);
    }

private:
    QToolButton *newActionButton(QnActions::IDType actionId, QWidget *parent = NULL) {
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
#ifdef _DEBUG
    connect(action(QnActions::DebugControlPanelAction),                &QAction::triggered,    this,   &QnWorkbenchDebugHandler::at_debugControlPanelAction_triggered);
    connect(action(QnActions::DebugIncrementCounterAction),            &QAction::triggered,    this,   &QnWorkbenchDebugHandler::at_debugIncrementCounterAction_triggered);
    connect(action(QnActions::DebugDecrementCounterAction),            &QAction::triggered,    this,   &QnWorkbenchDebugHandler::at_debugDecrementCounterAction_triggered);
    connect(action(QnActions::DebugShowResourcePoolAction),            &QAction::triggered,    this,   &QnWorkbenchDebugHandler::at_debugShowResourcePoolAction_triggered);
#endif
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

void QnWorkbenchDebugHandler::at_debugDecrementCounterAction_triggered()
{
    //menu()->trigger(QnActions::UserGroupsAction);
}

void QnWorkbenchDebugHandler::at_debugShowResourcePoolAction_triggered() {
    QScopedPointer<QnResourceListDialog> dialog(new QnResourceListDialog(mainWindow()));
    dialog->setResources(resourcePool()->getResources());
    dialog->exec();
}
