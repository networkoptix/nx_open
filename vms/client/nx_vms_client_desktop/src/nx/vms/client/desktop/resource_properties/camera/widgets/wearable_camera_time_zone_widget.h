#pragma once

#include <QtWidgets/QWidget>

#include <nx/utils/scoped_connections.h>

namespace Ui { class WearableCameraTimeZoneWidget; }

namespace nx::vms::client::desktop {

class CameraSettingsDialogStore;
struct CameraSettingsDialogState;

class WearableCameraTimeZoneWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit WearableCameraTimeZoneWidget(QWidget* parent = nullptr);
    virtual ~WearableCameraTimeZoneWidget() override;

    void setStore(CameraSettingsDialogStore* store);

private:
    void loadState(const CameraSettingsDialogState& state);

private:
    const QScopedPointer<Ui::WearableCameraTimeZoneWidget> ui;
    nx::utils::ScopedConnections m_storeConnections;
};

} // namespace nx::vms::client::desktop
