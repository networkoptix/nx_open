#pragma once

#include <QtWidgets/QDialog>

#include <licensing/license.h> // TODO: #Elric fwd
#include <ui/dialogs/common/session_aware_dialog.h>

class QnLicenseListModel;

namespace Ui {
    class LicenseNotificationDialog;
}

class QnLicenseNotificationDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    typedef QnSessionAwareButtonBoxDialog base_type;

public:
    QnLicenseNotificationDialog(QWidget* parent = nullptr, Qt::WindowFlags windowFlags = 0);
    virtual ~QnLicenseNotificationDialog();

    void setLicenses(const QList<QnLicensePtr>& licenses);

private:
    QScopedPointer<Ui::LicenseNotificationDialog> ui;
    QnLicenseListModel* m_model;
};
