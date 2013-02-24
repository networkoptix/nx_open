#include "system_health_popup_widget.h"
#include "ui_system_health_popup_widget.h"

#include <core/resource/user_resource.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameters.h>
#include <ui/workbench/workbench_context.h>

QnSystemHealthPopupWidget::QnSystemHealthPopupWidget(QWidget *parent) :
    QWidget(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnSystemHealthPopupWidget),
    m_messageType(QnSystemHealth::NotDefined)
{
    ui->setupUi(this);

    QPalette palette = this->palette();
    palette.setColor(QPalette::Window, QColor(85, 0, 0)); //TODO: #elric skin color
    this->setPalette(palette);

    connect(ui->fixButton,      SIGNAL(clicked()), this, SLOT(at_fixButton_clicked()));
    connect(ui->postponeButton, SIGNAL(clicked()), this, SLOT(at_postponeButton_clicked()));
}

QnSystemHealthPopupWidget::~QnSystemHealthPopupWidget()
{
}

bool QnSystemHealthPopupWidget::showSystemHealthMessage(QnSystemHealth::MessageType message) {

    m_messageType = message;
    if (message == QnSystemHealth::NotDefined)
        return false;
    ui->messageLabel->setText(QnSystemHealth::toString(m_messageType));

    return true;
}

void QnSystemHealthPopupWidget::at_fixButton_clicked() {
    switch (m_messageType) {
    case QnSystemHealth::EmailIsEmpty:
        menu()->trigger(Qn::UserSettingsAction,
                        QnActionParameters(context()->user()));
        break;
    case QnSystemHealth::NoLicenses:
        menu()->trigger(Qn::GetMoreLicensesAction);
        break;
    case QnSystemHealth::SmtpIsNotSet:
        menu()->trigger(Qn::OpenServerSettingsAction);
        break;
    default:
        break;
    }

    emit closed(m_messageType, ui->ignoreCheckBox->isChecked());
    hide();
}

void QnSystemHealthPopupWidget::at_postponeButton_clicked() {
    emit closed(m_messageType, ui->ignoreCheckBox->isChecked());
    hide();
}
