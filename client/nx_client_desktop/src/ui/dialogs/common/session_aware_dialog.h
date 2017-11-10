#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtCore/QSharedPointer>
#include <QtCore/QPointer>

#include <utils/common/forward.h>

#include <ui/dialogs/common/button_box_dialog.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/generic_tabbed_dialog.h>

#include <ui/workbench/workbench_state_manager.h>

#define COMMA ,
#define ID(x) x

template<class Base>
class QnSessionAware: public Base, public QnSessionAwareDelegate
{
    typedef Base base_type;
public:
    QN_FORWARD_CONSTRUCTOR(QnSessionAware, Base, ID(COMMA QnSessionAwareDelegate(this->parent()) {}))

    virtual bool tryClose(bool force) override
    {
        base_type::reject();
        if (force)
            base_type::hide();
        return true;
    }

    /** Forcibly update dialog contents. Default behavior is - simply close dialog. */
    virtual void forcedUpdate() override
    {
        retranslateUi();
        tryClose(true);
    }

protected:
    virtual void retranslateUi() {}
};

#undef ID
#undef COMMA

using QnSessionAwareMessageBox = QnSessionAware<QnMessageBox>;
using QnSessionAwareDialog = QnSessionAware<QnDialog>;
using QnSessionAwareFileDialog = QnSessionAware<QnCustomFileDialog>;

/**
 * Button box dialog that will be closed if we are disconnected from server.
 * Warning: class is QnWorkbenchContextAware
 */
class QnSessionAwareButtonBoxDialog: public QnButtonBoxDialog, public QnSessionAwareDelegate
{
    Q_OBJECT;
    typedef QnButtonBoxDialog base_type;

public:
    QnSessionAwareButtonBoxDialog(QWidget *parent, Qt::WindowFlags windowFlags = 0);

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
class QnSessionAwareTabbedDialog: public QnGenericTabbedDialog, public QnSessionAwareDelegate
{
    Q_OBJECT;
    typedef QnGenericTabbedDialog base_type;

public:
    QnSessionAwareTabbedDialog(QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0);

    virtual bool tryClose(bool force) override;

    /** Forcibly update dialog contents. */
    virtual void forcedUpdate() override;

protected:

    /**
     * @brief           Show the dialog, asking what to do with unsaved changes.
     * @returns         QDialogButtonBox::Yes if the changes should be saved
     *                  QDialogButtonBox::No if the changes should be discarded
     *                  QDialogButtonBox::Cancel to abort the process
     */
    virtual QDialogButtonBox::StandardButton showConfirmationDialog();
};
