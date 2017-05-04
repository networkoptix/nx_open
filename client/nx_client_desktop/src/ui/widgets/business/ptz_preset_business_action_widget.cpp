#include "ptz_preset_business_action_widget.h"
#include "ui_ptz_preset_business_action_widget.h"

#include <business/business_action_parameters.h>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <utils/common/scoped_value_rollback.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include "core/ptz/ptz_preset.h"
#include "core/ptz/ptz_controller_pool.h"
#include "core/ptz/fallback_ptz_controller.h"
#include "core/ptz/preset_ptz_controller.h"
#include "core/ptz/activity_ptz_controller.h"
#include "ui/fisheye/fisheye_ptz_controller.h"
#include "ui/workaround/widgets_signals_workaround.h"

QnExecPtzPresetBusinessActionWidget::QnExecPtzPresetBusinessActionWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::ExecPtzPresetBusinessActionWidget)
{
    ui->setupUi(this);
    connect(ui->presetComboBox,  QnComboboxCurrentIndexChanged, this, &QnExecPtzPresetBusinessActionWidget::paramsChanged);
    //setHelpTopic(this, Qn::EventsActions_CameraOutput_Help);
}

QnExecPtzPresetBusinessActionWidget::~QnExecPtzPresetBusinessActionWidget()
{
}

void QnExecPtzPresetBusinessActionWidget::updateTabOrder(QWidget *before, QWidget *after)
{
    setTabOrder(before, ui->presetComboBox);
    setTabOrder(ui->presetComboBox, after);
}

void QnExecPtzPresetBusinessActionWidget::at_model_dataChanged(QnBusiness::Fields fields) {
    if (!model())
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    if (fields & QnBusiness::ActionResourcesField)
    {
        ui->presetComboBox->clear();
        m_presets.clear();
        m_ptzController.reset();
        auto cameras = resourcePool()->getResources<QnVirtualCameraResource>(model()->actionResources());
        if (cameras.size() != 1)
            return; // single camera only allowed
        setupPtzController(cameras[0]);
    }

    if (fields & (QnBusiness::ActionParamsField | QnBusiness::ActionResourcesField))
        updateComboboxItems(model()->actionParams().presetId);
}

void QnExecPtzPresetBusinessActionWidget::setupPtzController(const QnVirtualCameraResourcePtr& camera)
{
    if (m_ptzController)
        disconnect(m_ptzController.data(), nullptr, this, nullptr);

    QnPtzControllerPtr fisheyeController;
    fisheyeController.reset(new QnFisheyePtzController(camera), &QObject::deleteLater);
    fisheyeController.reset(new QnPresetPtzController(fisheyeController));

    if (QnPtzControllerPtr serverController = qnPtzPool->controller(camera))
    {
        serverController.reset(new QnActivityPtzController(commonModule(),
            QnActivityPtzController::Client, serverController));
        m_ptzController.reset(new QnFallbackPtzController(fisheyeController, serverController));
    }
    else
    {
        m_ptzController = fisheyeController;
    }

    connect(m_ptzController.data(), &QnAbstractPtzController::changed, this,
        [this](Qn::PtzDataFields fields)
        {
            if (fields & Qn::PresetsPtzField)
                updateComboboxItems(ui->presetComboBox->currentData().toString());
        });
    m_ptzController->getPresets(&m_presets);
}

void QnExecPtzPresetBusinessActionWidget::updateComboboxItems(const QString& presetId)
{
    ui->presetComboBox->clear();
    for (const auto& preset: m_presets)
        ui->presetComboBox->addItem(preset.name, preset.id);

    if (ui->presetComboBox->currentData().toString() != presetId)
        ui->presetComboBox->setCurrentIndex(ui->presetComboBox->findData(presetId));
};

void QnExecPtzPresetBusinessActionWidget::paramsChanged()
{
    if (!model() || m_updating)
        return;

    QnBusinessActionParameters params = model()->actionParams();
    params.presetId = ui->presetComboBox->itemData(ui->presetComboBox->currentIndex()).toString();
    model()->setActionParams(params);
}
