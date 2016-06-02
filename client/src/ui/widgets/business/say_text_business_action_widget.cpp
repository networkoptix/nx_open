#include <core/resource/camera_resource.h>
#include "say_text_business_action_widget.h"
#include "ui_say_text_business_action_widget.h"

#include <business/business_action_parameters.h>
#include <ui/dialogs/resource_selection_dialog.h>
#include <ui/style/resource_icon_cache.h>

#include <openal/qtvaudiodevice.h>

#include <utils/common/scoped_value_rollback.h>
#include <utils/media/audio_player.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <core/resource_management/resource_pool.h>

QnSayTextBusinessActionWidget::QnSayTextBusinessActionWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::SayTextBusinessActionWidget)
{
    ui->setupUi(this);

    ui->volumeSlider->setValue(qRound(QtvAudioDevice::instance()->volume() * 100));
    ui->actionTargetsHolder->setIcon(qnResIconCache->icon(QnResourceIconCache::Camera));

    connect(ui->textEdit, SIGNAL(textChanged(QString)), this, SLOT(paramsChanged()));
    connect(ui->playToClient, SIGNAL(stateChanged(int)), this, SLOT(paramsChanged()));
    connect(ui->testButton, SIGNAL(clicked()), this, SLOT(at_testButton_clicked()));
    connect(ui->volumeSlider, SIGNAL(valueChanged(int)), this, SLOT(at_volumeSlider_valueChanged(int)));
    connect(ui->actionTargetsHolder, SIGNAL(clicked()), this, SLOT(at_actionTargetsHolder_clicked()));

    connect(QtvAudioDevice::instance(), &QtvAudioDevice::volumeChanged, this, [this] {
        QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
        ui->volumeSlider->setValue(qRound(QtvAudioDevice::instance()->volume() * 100));
    });

    setHelpTopic(this, Qn::EventsActions_Speech_Help);
}

QnSayTextBusinessActionWidget::~QnSayTextBusinessActionWidget()
{
}

void QnSayTextBusinessActionWidget::updateTabOrder(QWidget *before, QWidget *after) {
    setTabOrder(before,             ui->textEdit);
    setTabOrder(ui->textEdit,       ui->volumeSlider);
    setTabOrder(ui->volumeSlider,   ui->testButton);
    setTabOrder(ui->testButton,     after);
}

QString QnSayTextBusinessActionWidget::getActionTargetsHolderText(
    const QnBusinessActionParameters& params) const
{
    auto resources = qnResPool->getResources<QnResource>(params.additionalResources);

    if(resources.empty())
        return tr("<Choose camera>");

    auto errorText = QnCameraAudioTransmitPolicy::getText(resources);
    if (!errorText.isEmpty())
        return errorText;

    if (resources.size() == 1)
        return resources[0]->getName();
    else
        return tr("%1 Camera(s)").arg(resources.size());
}

void QnSayTextBusinessActionWidget::at_model_dataChanged(QnBusiness::Fields fields) {
    if (!model() || m_updating)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    auto params = model()->actionParams();
    if (fields & QnBusiness::ActionParamsField)
    {
        ui->textEdit->setText(params.sayText);
        ui->playToClient->setChecked(params.playToClient);
        ui->actionTargetsHolder->setText(getActionTargetsHolderText(params));
    }
}

void QnSayTextBusinessActionWidget::paramsChanged() {
    if (!model() || m_updating)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    auto params = model()->actionParams();
    params.sayText = ui->textEdit->text();
    params.playToClient = ui->playToClient->isChecked();
    model()->setActionParams(params);
}

void QnSayTextBusinessActionWidget::enableTestButton() {
    ui->testButton->setEnabled(true);
}

void QnSayTextBusinessActionWidget::at_testButton_clicked() {
    if (ui->textEdit->text().isEmpty())
        return;
    if (AudioPlayer::sayTextAsync(ui->textEdit->text(), this, SLOT(enableTestButton())))
        ui->testButton->setEnabled(false);
}

void QnSayTextBusinessActionWidget::at_volumeSlider_valueChanged(int value) {
    if (m_updating)
        return;

    QtvAudioDevice::instance()->setVolume((qreal)value * 0.01);
}

void QnSayTextBusinessActionWidget::at_actionTargetsHolder_clicked() {

    auto params = model()->actionParams();
    QnResourceSelectionDialog dialog(this);
    QnResourceList selected = qnResPool->getResources(params.additionalResources);

    dialog.setDelegate(
        new QnCheckResourceAndWarnDelegate<QnCameraAudioTransmitPolicy>(this));
    dialog.setSelectedResources(selected);

    if (dialog.exec() != QDialog::Accepted)
        return;

    params.additionalResources.clear();
    for(const auto& res: dialog.selectedResources())
        params.additionalResources.push_back(res->getId());

    model()->setActionParams(params);
}
