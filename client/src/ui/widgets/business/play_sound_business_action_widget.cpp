#include "play_sound_business_action_widget.h"
#include "ui_play_sound_business_action_widget.h"

#include <business/business_action_parameters.h>

#include <ui/dialogs/notification_sound_manager_dialog.h>
#include <ui/workbench/workbench_context.h>

#include <utils/common/scoped_value_rollback.h>

QnPlaySoundBusinessActionWidget::QnPlaySoundBusinessActionWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnPlaySoundBusinessActionWidget)
{
    ui->setupUi(this);

    connect(ui->pathComboBox, SIGNAL(editTextChanged(QString)), this, SLOT(paramsChanged()));
    connect(ui->manageButton, SIGNAL(clicked()), this, SLOT(at_manageButton_clicked()));
}

QnPlaySoundBusinessActionWidget::~QnPlaySoundBusinessActionWidget()
{
}


void QnPlaySoundBusinessActionWidget::at_model_dataChanged(QnBusinessRuleViewModel *model, QnBusiness::Fields fields) {
    if (!model)
        return;

    QnScopedValueRollback<bool> guard(&m_updating, true);
    Q_UNUSED(guard)

    if (fields & QnBusiness::ActionParamsField) {
        QString path = QnBusinessActionParameters::getSoundUrl(model->actionParams());
        if (ui->pathComboBox->currentText() != path)
            ui->pathComboBox->setEditText(path);
    }
}

void QnPlaySoundBusinessActionWidget::paramsChanged() {
    if (!model() || m_updating)
        return;

    QnBusinessParams params;
    QnBusinessActionParameters::setSoundUrl(&params, ui->pathComboBox->currentText());
    model()->setActionParams(params);
}

void QnPlaySoundBusinessActionWidget::at_manageButton_clicked() {
    QScopedPointer<QnNotificationSoundManagerDialog> dialog(new QnNotificationSoundManagerDialog());
    dialog->exec();
}
