#include "play_sound_business_action_widget.h"
#include "ui_play_sound_business_action_widget.h"

#include <business/business_action_parameters.h>

#include <ui/dialogs/notification_sound_manager_dialog.h>
#include <ui/workbench/workbench_context.h>

#include <utils/common/scoped_value_rollback.h>
#include <utils/app_server_notification_cache.h>

QnPlaySoundBusinessActionWidget::QnPlaySoundBusinessActionWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnPlaySoundBusinessActionWidget),
    m_cache(new QnAppServerNotificationCache(this)),
    m_loaded(false)
{
    ui->setupUi(this);

    connect(ui->pathComboBox, SIGNAL(editTextChanged(QString)), this, SLOT(paramsChanged()));
    connect(ui->manageButton, SIGNAL(clicked()), this, SLOT(at_manageButton_clicked()));

    connect(m_cache, SIGNAL(fileListReceived(QStringList,bool)), this, SLOT(at_fileListReceived(QStringList,bool)));
    m_cache->getFileList();
}

QnPlaySoundBusinessActionWidget::~QnPlaySoundBusinessActionWidget()
{
}

void QnPlaySoundBusinessActionWidget::updateSoundUrl() {
    QnScopedValueRollback<bool> guard(&m_updating, true);
    Q_UNUSED(guard)

    int idx = ui->pathComboBox->findText(m_soundUrl);
    if (idx > 0)
        ui->pathComboBox->setCurrentIndex(idx);
    else
        ui->pathComboBox->setCurrentIndex(0);
}

void QnPlaySoundBusinessActionWidget::at_model_dataChanged(QnBusinessRuleViewModel *model, QnBusiness::Fields fields) {
    if (!model)
        return;

    if (fields & QnBusiness::ActionParamsField) {
        m_soundUrl = QnBusinessActionParameters::getSoundUrl(model->actionParams());
        if (m_loaded)
            updateSoundUrl();
    }
}

void QnPlaySoundBusinessActionWidget::paramsChanged() {
    if (!model() || m_updating || !m_loaded)
        return;

    QnBusinessParams params;
    QnBusinessActionParameters::setSoundUrl(&params, ui->pathComboBox->currentText());
    model()->setActionParams(params);
}



void QnPlaySoundBusinessActionWidget::at_manageButton_clicked() {
    QScopedPointer<QnNotificationSoundManagerDialog> dialog(new QnNotificationSoundManagerDialog());
    dialog->exec();
}

void QnPlaySoundBusinessActionWidget::at_fileListReceived(const QStringList &filenames, bool ok) {
    ui->pathComboBox->clear();
    if (!ok) {
        ui->pathComboBox->addItem(tr("<Connection error>"));
        ui->pathComboBox->setCurrentIndex(0);
        return;
    }

    ui->pathComboBox->addItem(tr("<No sound>"));
    foreach(const QString &filename, filenames) {
        ui->pathComboBox->addItem(filename); //TODO: #GDM metatada, filename in userData
    }
    m_loaded = true;
    updateSoundUrl();

}
