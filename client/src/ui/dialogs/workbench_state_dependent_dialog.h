#ifndef QN_WORKBENCH_STATE_DEPENDENT_DIALOG_H
#define QN_WORKBENCH_STATE_DEPENDENT_DIALOG_H

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtCore/QSharedPointer>
#include <QtCore/QPointer>

#include <utils/common/forward.h>

#include <ui/dialogs/button_box_dialog.h>
#include <ui/dialogs/generic_tabbed_dialog.h>

#include <ui/workbench/workbench_state_manager.h>

#define COMMA ,
#define ID(x) x

template<class Base>
class QnWorkbenchStateDependentDialog: public Base, public QnWorkbenchStateDelegate  {
    typedef Base base_type;
public:
    QN_FORWARD_CONSTRUCTOR(QnWorkbenchStateDependentDialog, Base, ID(COMMA QnWorkbenchStateDelegate(this->parent()) {}))

    virtual bool tryClose(bool force) override {
        base_type::reject();
        if (force)
            base_type::hide();
        return true;
    }

    /** Forcibly update dialog contents. Default behavior is - simply close dialog. */
    virtual void forcedUpdate() override {
        retranslateUi();
        tryClose(true);
    }

protected:
    virtual void retranslateUi() {}
};

#undef ID
#undef COMMA

/**
 * Button box dialog that will be closed if we are disconnected from server.
 * Warning: class is QnWorkbenchContextAware
 */
class QnWorkbenchStateDependentButtonBoxDialog: public QnButtonBoxDialog, public QnWorkbenchStateDelegate {
    Q_OBJECT;
    typedef QnButtonBoxDialog base_type;

public:
    QnWorkbenchStateDependentButtonBoxDialog(QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0);

    virtual bool tryClose(bool force) override;

    /** Forcibly update dialog contents. Default behavior is - simply close dialog. */
    virtual void forcedUpdate() override;

protected:
    virtual void retranslateUi() {}
};

/**
 * Tabbed dialog that will be closed if we are disconnected from server.
 * Warning: class is QnWorkbenchContextAware
 */
class QnWorkbenchStateDependentTabbedDialog: public QnGenericTabbedDialog, public QnWorkbenchStateDelegate {
    Q_OBJECT;
    typedef QnGenericTabbedDialog base_type;

public:
    QnWorkbenchStateDependentTabbedDialog(QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0);

    virtual bool tryClose(bool force) override;

    /** Forcibly update dialog contents. */
    virtual void forcedUpdate() override;

protected:

    /**
     * @brief           Show the dialog, asking what to do with unsaved changes.
     * @returns         QMessageBox::Yes if the changes should be saved
     *                  QMessageBox::No if the changes should be discarded
     *                  QMessageBox::Cancel to abort the process
     */
    virtual QMessageBox::StandardButton showConfirmationDialog();
};


#endif // QN_WORKBENCH_STATE_DEPENDENT_DIALOG_H
