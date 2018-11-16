#pragma once

#include <QtWidgets/QWidget>

#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/utils/wearable_fwd.h>

namespace Ui { class WearableCameraUploadWidget; }

namespace nx::vms::client::desktop {

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
    void actionRequested(nx::vms::client::desktop::ui::action::IDType action);

private:
    void loadState(const CameraSettingsDialogState& state);

private:
    const QScopedPointer<Ui::WearableCameraUploadWidget> ui;
    nx::utils::ScopedConnections m_storeConnections;
};

} // namespace nx::vms::client::desktop
