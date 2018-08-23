#pragma once

#include <core/resource/resource.h>
#include <ui/dialogs/common/session_aware_dialog.h>

#include <nx/client/desktop/node_view/details/node_view_fwd.h>

namespace Ui { class CameraSelectionDialog; }

namespace nx::client::desktop {

using ValidResourceCheck = std::function<bool (const QnResourcePtr& resource)>;
class CameraSelectionDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

    static bool selectCamerasInternal(
        const ValidResourceCheck& validResourceCheck,
        node_view::details::UuidSet& selectedCameras,
        QWidget* parent);

public:
    template<typename ResourcePolicy>
    static bool selectCameras(
        node_view::details::UuidSet& selectedCameras,
        QWidget* parent)
    {
        const auto validResourceCheck =
            [](const QnResourcePtr& resource)
            {
                const auto target = resource.dynamicCast<typename ResourcePolicy::resource_type>();
                return ResourcePolicy::isResourceValid(target);
            };

        return selectCamerasInternal(validResourceCheck, selectedCameras, parent);
    }

private:
    CameraSelectionDialog(
        const ValidResourceCheck& validResourceCheck,
        const node_view::details::UuidSet& selectedCameras,
        QWidget* parent);

    virtual ~CameraSelectionDialog() override;

private:
    struct Private;
    const QScopedPointer<Private> d;
    const QScopedPointer<Ui::CameraSelectionDialog> ui;
};

} // namespace nx::client::desktop
