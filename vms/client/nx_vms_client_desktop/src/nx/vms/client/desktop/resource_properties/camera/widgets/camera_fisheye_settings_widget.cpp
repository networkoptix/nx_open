#include "camera_fisheye_settings_widget.h"

#include <QtWidgets/QVBoxLayout>

#include "../redux/camera_settings_dialog_state.h"
#include "../redux/camera_settings_dialog_store.h"

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/helper.h>

#include <nx/vms/client/desktop/resource_properties/fisheye/fisheye_settings_widget.h>

namespace nx::vms::client::desktop {

CameraFisheyeSettingsWidget::CameraFisheyeSettingsWidget(
    QSharedPointer<ImageProvider> previewProvider,
    CameraSettingsDialogStore* store,
    QWidget* parent)
    :
    base_type(parent),
    m_previewProvider(previewProvider),
    m_widget(new FisheyeSettingsWidget(this))
{
    NX_ASSERT(previewProvider, "Preview image provider must be set", Q_FUNC_INFO);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(style::Metrics::kDefaultTopLevelMargins);
    layout->addWidget(m_widget);

    setHelpTopic(this, Qn::CameraSettings_Dewarping_Help);

    connect(store, &CameraSettingsDialogStore::stateChanged, this,
        &CameraFisheyeSettingsWidget::loadState);

    connect(m_widget, &FisheyeSettingsWidget::dataChanged, this,
        [this, store]()
        {
            QnMediaDewarpingParams params;
            m_widget->submitToParams(params);
            store->setFisheyeSettings(params);
        });
}

CameraFisheyeSettingsWidget::~CameraFisheyeSettingsWidget()
{
}

void CameraFisheyeSettingsWidget::loadState(const CameraSettingsDialogState& state)
{
    m_widget->updateFromParams(state.singleCameraSettings.fisheyeDewarping(),
        m_previewProvider.data());
    m_widget->setReadOnly(state.readOnly);
}

} // namespace nx::vms::client::desktop
