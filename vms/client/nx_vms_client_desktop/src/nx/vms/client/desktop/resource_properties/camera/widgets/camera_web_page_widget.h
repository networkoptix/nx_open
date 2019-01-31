#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <common/common_module_aware.h>

namespace nx::vms::client::desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;

class CameraWebPageWidget:
    public QWidget,
    public QnCommonModuleAware
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit CameraWebPageWidget(
        CameraSettingsDialogStore* store,
        QWidget* parent = nullptr);

    virtual ~CameraWebPageWidget() override;

private:
    void loadState(const CameraSettingsDialogState& state);

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
