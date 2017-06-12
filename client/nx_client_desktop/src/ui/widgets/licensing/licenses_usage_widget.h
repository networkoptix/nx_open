#pragma once

#include <QtWidgets/QWidget>

#include <common/common_globals.h>

class QnLicenseUsageWidgetRow;
class QnLicenseUsageHelper;

class QnLicensesUsageWidget: public QWidget {
    Q_OBJECT
public:
    QnLicensesUsageWidget(QWidget *parent);
    ~QnLicensesUsageWidget();

    void init(QnLicenseUsageHelper* helper);
    void loadData(QnLicenseUsageHelper* helper);

private:
    QHash<Qn::LicenseType, QnLicenseUsageWidgetRow*> m_rows;

};
