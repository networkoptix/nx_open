#ifndef QN_LICENSE_NOTIFICATION_DIALOG_H
#define QN_LICENSE_NOTIFICATION_DIALOG_H

#include <QtGui/QDialog>
#include <licensing/license.h> // TODO: #Elric fwd

class QnLicenseListModel;

namespace Ui {
    class LicenseNotificationDialog;
}

class QnLicenseNotificationDialog: public QDialog {
    Q_OBJECT
    typedef QDialog base_type;

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
