#pragma once

#include <QtWidgets/QWidget>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <client_core/connection_context_aware.h>
#include <nx/utils/uuid.h>

namespace Ui { class CameraExpertSettingsWidget; }

namespace nx::vms::client::desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;

class CameraExpertSettingsWidget:
    public QWidget,
    public QnConnectionContextAware
{
    Q_OBJECT
    using base_type = QWidget;

public:
    CameraExpertSettingsWidget(CameraSettingsDialogStore* store, QWidget* parent = nullptr);
    virtual ~CameraExpertSettingsWidget() override;

private:
    void loadState(const CameraSettingsDialogState& state);

private:
    const QScopedPointer<Ui::CameraExpertSettingsWidget> ui;
};

} // namespace nx::vms::client::desktop
