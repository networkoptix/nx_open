#pragma once

#include <QtCore/QSharedPointer>
#include <QtWidgets/QWidget>

class QnImageProvider;

namespace nx {
namespace client {
namespace desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;

class FisheyeSettingsWidget;

class CameraFisheyeSettingsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit CameraFisheyeSettingsWidget(QSharedPointer<QnImageProvider> previewProvider,
        CameraSettingsDialogStore* store, QWidget* parent = nullptr);

    virtual ~CameraFisheyeSettingsWidget() override;

private:
    void loadState(const CameraSettingsDialogState& state);

private:
    QSharedPointer<QnImageProvider> m_previewProvider;
    FisheyeSettingsWidget* const m_widget = nullptr;
};

} // namespace desktop
} // namespace client
} // namespace nx
