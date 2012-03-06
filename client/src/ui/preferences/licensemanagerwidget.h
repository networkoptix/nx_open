#ifndef LICENSEMANAGERWIDGET_H
#define LICENSEMANAGERWIDGET_H

#include <QtGui/QWidget>

#include "licensing/license.h"

namespace Ui {
    class LicenseManagerWidget;
}

class LicenseManagerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LicenseManagerWidget(QWidget *parent = 0);
    ~LicenseManagerWidget();

private slots:
    void licensesChanged();
    void licenseDetailsButtonClicked();
    void licensesReceived(int status, const QByteArray& errorString, QnLicenseList licenses, int handle);
    void gridSelectionChanged();

private:
    void updateControls();

private:
    Q_DISABLE_COPY(LicenseManagerWidget)

    QScopedPointer<Ui::LicenseManagerWidget> ui;
    QnLicenseList m_licenses;
};

#endif // LICENSEMANAGERWIDGET_H
