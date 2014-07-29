#ifndef QN_LICENSE_NOTIFICATION_DIALOG_H
#define QN_LICENSE_NOTIFICATION_DIALOG_H

#include <QtWidgets/QDialog>
#include <licensing/license.h> // TODO: #Elric fwd

#include <ui/dialogs/workbench_state_dependent_dialog.h>

class QnLicenseListModel;

namespace Ui {
    class LicenseNotificationDialog;
}

class QnLicenseNotificationDialog: public QnWorkbenchStateDependentButtonBoxDialog {
    Q_OBJECT

    typedef QnWorkbenchStateDependentButtonBoxDialog base_type;

public:
    QnLicenseNotificationDialog(QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnLicenseNotificationDialog();

    const QList<QnLicensePtr> &licenses() const;
    void setLicenses(const QList<QnLicensePtr> &licenses);

private:
    QScopedPointer<Ui::LicenseNotificationDialog> ui;
    QnLicenseListModel *m_model;
};

#endif // QN_LICENSE_NOTIFICATION_DIALOG_H
