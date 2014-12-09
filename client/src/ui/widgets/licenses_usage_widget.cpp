#include "licenses_usage_widget.h"

#include <ui/style/warning_style.h>

#include <utils/license_usage_helper.h>

class QnLicenseUsageWidgetRow: public QWidget {
public:
    QnLicenseUsageWidgetRow(QWidget* parent):
        QWidget(parent),
        m_usageLabel(new QLabel(this)),
        m_requiredLabel(new QLabel(this))
    {

        QHBoxLayout *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(m_usageLabel);
        layout->addStretch();
        layout->addWidget(m_requiredLabel);
        setLayout(layout);
    }

    void setValues(const QString &usage, const QString &required) {
        m_usageLabel->setText(usage);
        m_requiredLabel->setText(required);
    }

    void setValid(bool valid) {
        QPalette palette = parentWidget()->palette();
        if (!valid)
            setWarningStyle(&palette);
        setPalette(palette);
    }

private:
    QLabel* m_usageLabel;
    QLabel* m_requiredLabel;

};


QnLicensesUsageWidget::QnLicensesUsageWidget(QWidget *parent): 
    QWidget(parent) {}

QnLicensesUsageWidget::~QnLicensesUsageWidget() {

}

void QnLicensesUsageWidget::init(QnLicenseUsageHelper* helper) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    foreach (Qn::LicenseType lt, helper->licenseTypes()) {
        QnLicenseUsageWidgetRow* row  = new QnLicenseUsageWidgetRow(this);
        layout->addWidget(row);
        m_rows.insert(lt, row);
        row->setVisible(false);
    }
    setLayout(layout);
}

void QnLicensesUsageWidget::loadData(QnLicenseUsageHelper* helper) {
    foreach (Qn::LicenseType lt, helper->licenseTypes()) {
        QnLicenseUsageWidgetRow* row = dynamic_cast<QnLicenseUsageWidgetRow*>(m_rows[lt]);
        Q_ASSERT(row);
        QString licenseText = helper->getUsageText(lt);
        row->setVisible(!licenseText.isEmpty());
        if (licenseText.isEmpty())
            continue;

        QString requiredText = helper->getRequiredLicenseMsg(lt);
        //ui->enableRecordingCheckBox->checkState() == Qt::Checked
        //    : QString();
        row->setValues(licenseText, requiredText);
        row->setValid(helper->isValid(lt));
    }
}

