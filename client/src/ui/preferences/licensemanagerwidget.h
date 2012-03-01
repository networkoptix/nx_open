#ifndef LICENSEMANAGERWIDGET_H
#define LICENSEMANAGERWIDGET_H

#include <QtGui/QWidget>

namespace Ui {
    class LicenseManagerWidget;
}

class LicenseManagerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LicenseManagerWidget(QWidget *parent = 0);
    ~LicenseManagerWidget();

private:
    void updateControls();

private:
    Q_DISABLE_COPY(LicenseManagerWidget)

    QScopedPointer<Ui::LicenseManagerWidget> ui;
};

#endif // LICENSEMANAGERWIDGET_H
