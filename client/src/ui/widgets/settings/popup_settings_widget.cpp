#include "popup_settings_widget.h"
#include "ui_popup_settings_widget.h"

#include <utils/settings.h>
#include <business/events/abstract_business_event.h>

QnPopupSettingsWidget::QnPopupSettingsWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QnPopupSettingsWidget)
{
    ui->setupUi(this);

    for (int i = 0; i < BusinessEventType::BE_Count; i++) {
        QCheckBox* checkbox = new QCheckBox(this);
        checkbox->setText(BusinessEventType::toString(BusinessEventType::Value(i)));
        ui->checkBoxesLayout->addWidget(checkbox);
        m_checkBoxes << checkbox;
    }

    connect(ui->showAllCheckBox, SIGNAL(toggled(bool)), this, SLOT(at_showAllCheckBox_toggled(bool)));
}

QnPopupSettingsWidget::~QnPopupSettingsWidget()
{
}

void QnPopupSettingsWidget::updateFromSettings(QnSettings *settings) {
    quint64 shown = settings->shownPopups();
    quint64 flag = 1;
    bool all = true;

    for (int i = 0; i < BusinessEventType::BE_Count; i++) {
        bool checked = shown & flag;
        m_checkBoxes[i]->setChecked(checked);
        all = all && checked;
        flag = flag << 1;
    }

    ui->showAllCheckBox->setChecked(all);
}

void QnPopupSettingsWidget::submitToSettings(QnSettings *settings) {
    quint64 shown = settings->shownPopups();
    quint64 flag = 1;
    for (int i = 0; i < BusinessEventType::BE_Count; i++) {
        if (m_checkBoxes[i]->isChecked() || ui->showAllCheckBox->isChecked()) {
            shown |= flag;
        } else {
            shown &= ~flag;
        }
        flag = flag << 1;
    }

    settings->setShownPopups(shown);
}

void QnPopupSettingsWidget::at_showAllCheckBox_toggled(bool checked) {
    // TODO: #GDM also update checked state!
    // TODO: maybe tristate for 'show all' checkbox would be better.
    foreach (QCheckBox* checkbox, m_checkBoxes) {
        checkbox->setEnabled(!checked);
    }
}
