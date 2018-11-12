#pragma once

#include <QtCore/QSharedPointer>
#include <QtWidgets/QWidget>

namespace nx::vms::client::desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;

class FisheyeSettingsWidget;
class ImageProvider;

class CameraFisheyeSettingsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit CameraFisheyeSettingsWidget(QSharedPointer<ImageProvider> previewProvider,
        CameraSettingsDialogStore* store, QWidget* parent = nullptr);

    virtual ~CameraFisheyeSettingsWidget() override;

private:
    void loadState(const CameraSettingsDialogState& state);

private:
    QSharedPointer<ImageProvider> m_previewProvider;
    FisheyeSettingsWidget* const m_widget = nullptr;
};

} // namespace nx::vms::client::desktop
