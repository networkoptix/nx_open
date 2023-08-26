// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ptz_preset_business_action_widget.h"
#include "ui_ptz_preset_business_action_widget.h"

#include <QtCore/QScopedValueRollback>

#include <client_core/client_core_module.h>
#include <core/ptz/activity_ptz_controller.h>
#include <core/ptz/fallback_ptz_controller.h>
#include <core/ptz/preset_ptz_controller.h>
#include <core/ptz/ptz_controller_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/event/action_parameters.h>
#include <ui/fisheye/fisheye_ptz_controller.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include "core/ptz/ptz_preset.h"

using namespace nx::vms::client::desktop;

QnExecPtzPresetBusinessActionWidget::QnExecPtzPresetBusinessActionWidget(
    SystemContext* systemContext,
    QWidget* parent)
    :
    base_type(systemContext, parent),
    ui(new Ui::ExecPtzPresetBusinessActionWidget)
{
    ui->setupUi(this);
    connect(ui->presetComboBox,  QnComboboxCurrentIndexChanged, this, &QnExecPtzPresetBusinessActionWidget::paramsChanged);
    //setHelpTopic(this, HelpTopic::Id::EventsActions_CameraOutput);
}

QnExecPtzPresetBusinessActionWidget::~QnExecPtzPresetBusinessActionWidget()
{
}

void QnExecPtzPresetBusinessActionWidget::updateTabOrder(QWidget *before, QWidget *after)
{
    setTabOrder(before, ui->presetComboBox);
    setTabOrder(ui->presetComboBox, after);
}

void QnExecPtzPresetBusinessActionWidget::at_model_dataChanged(Fields fields) {
    if (!model())
        return;

    QScopedValueRollback<bool> guard(m_updating, true);

    if (fields.testFlag(Field::actionResources))
    {
        ui->presetComboBox->clear();
        m_presets.clear();
        m_ptzController.reset();
        auto cameras = resourcePool()->getResourcesByIds<QnVirtualCameraResource>(model()->actionResources());
        if (cameras.size() != 1)
            return; // single camera only allowed
        setupPtzController(cameras[0]);
    }

    if (fields & (Field::actionParams | Field::actionResources))
        updateComboboxItems(model()->actionParams().presetId);
}

void QnExecPtzPresetBusinessActionWidget::setupPtzController(const QnVirtualCameraResourcePtr& camera)
{
    if (m_ptzController)
        m_ptzController->disconnect(this);

    auto fisheyeController =
        QSharedPointer<QnPresetPtzController>::create(QSharedPointer<QnFisheyePtzController>(
            new QnFisheyePtzController(camera), &QObject::deleteLater));

    auto systemContext = SystemContext::fromResource(camera);
    auto ptzPool = systemContext->ptzControllerPool();
    if (QnPtzControllerPtr serverController = ptzPool->controller(camera))
    {
        serverController.reset(new QnActivityPtzController(
            QnActivityPtzController::Client,
            serverController));
        m_ptzController.reset(new QnFallbackPtzController(fisheyeController, serverController));
    }
    else
    {
        m_ptzController = fisheyeController;
    }

    connect(m_ptzController.data(), &QnAbstractPtzController::changed, this,
        [this](nx::vms::common::ptz::DataFields fields)
        {
            if (fields.testFlag(nx::vms::common::ptz::DataField::presets))
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

    nx::vms::event::ActionParameters params = model()->actionParams();
    params.presetId = ui->presetComboBox->itemData(ui->presetComboBox->currentIndex()).toString();
    model()->setActionParams(params);
}
