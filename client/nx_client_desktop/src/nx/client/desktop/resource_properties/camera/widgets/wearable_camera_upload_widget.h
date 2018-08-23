#pragma once

#include <QtWidgets/QWidget>

#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/utils/wearable_fwd.h>

class QnDisconnectHelper;
namespace Ui { class WearableCameraUploadWidget; }

namespace nx {
namespace client {
namespace desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;

class WearableCameraUploadWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit WearableCameraUploadWidget(QWidget* parent = nullptr);
    virtual ~WearableCameraUploadWidget() override;

    void setStore(CameraSettingsDialogStore* store);

signals:
    void actionRequested(nx::client::desktop::ui::action::IDType action);

private:
    void loadState(const CameraSettingsDialogState& state);

private:
    const QScopedPointer<Ui::WearableCameraUploadWidget> ui;
    QScopedPointer<QnDisconnectHelper> m_storeConnections;
};

} // namespace desktop
} // namespace client
} // namespace nx
