#ifndef QN_LICENSES_USAGE_WIDGET_H
#define QN_LICENSES_USAGE_WIDGET_H

#include <QtWidgets/QWidget>

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


#endif //QN_LICENSES_USAGE_WIDGET_H