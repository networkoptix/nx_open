#include "popup_settings_widget.h"
#include "ui_popup_settings_widget.h"

#include <utils/settings.h>
#include <events/abstract_business_event.h>

QnPopupSettingsWidget::QnPopupSettingsWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QnPopupSettingsWidget)
{
    ui->setupUi(this);

    for (int i = 0; i < BusinessEventType::BE_Count; i++) {
        QCheckBox* checkbox = new QCheckBox(this);
        checkbox->setText(BusinessEventType::toString(BusinessEventType::Value(i)));
        ui->ignoreLayout->addWidget(checkbox);
        m_checkBoxes << checkbox;
    }

    connect(ui->ignoreAllCheckBox, SIGNAL(toggled(bool)), this, SLOT(at_ignoreAllCheckBox_toggled(bool)));

}

QnPopupSettingsWidget::~QnPopupSettingsWidget()
{
    delete ui;
}

void QnPopupSettingsWidget::updateFromSettings(QnSettings *settings) {
    quint64 ignored = settings->ignorePopupFlags();
    quint64 flag = 1;
    bool all = true;

    for (int i = 0; i < BusinessEventType::BE_Count; i++) {
        bool checked = ignored & flag;
        m_checkBoxes[i]->setChecked(checked);
        all = all && checked;
        flag = flag << 1;
    }

    ui->ignoreAllCheckBox->setChecked(all);
}

void QnPopupSettingsWidget::submitToSettings(QnSettings *settings) {
    quint64 ignored = 0;
    quint64 flag = 1;
    for (int i = 0; i < BusinessEventType::BE_Count; i++) {
        if (m_checkBoxes[i]->isChecked() || ui->ignoreAllCheckBox->isChecked())
            ignored |= flag;
        flag = flag << 1;
    }

    settings->setIgnorePopupFlags(ignored);
}

void QnPopupSettingsWidget::at_ignoreAllCheckBox_toggled(bool checked) {

    foreach (QCheckBox* checkbox, m_checkBoxes) {
        checkbox->setEnabled(!checked);
    }

}
