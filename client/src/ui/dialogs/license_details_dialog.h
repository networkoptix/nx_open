#ifndef QN_LICENSE_DETAILS_DIALOG_H
#define QN_LICENSE_DETAILS_DIALOG_H

#include <QtWidgets/QDialog>
#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/workbench_state_dependent_dialog.h>

namespace Ui {
    class LicenseDetailsDialog;
}

class QnLicenseDetailsDialog: public QnWorkbenchStateDependentButtonBoxDialog {
    Q_OBJECT

    typedef QnWorkbenchStateDependentButtonBoxDialog base_type;
public:
    QnLicenseDetailsDialog(const QnLicensePtr &license, QWidget *parent = NULL);
    virtual ~QnLicenseDetailsDialog();

private:
    QString licenseDescription(const QnLicensePtr &license) const;
private:
    QScopedPointer<Ui::LicenseDetailsDialog> ui;
};

#endif // QN_LICENSE_DETAILS_DIALOG_H
