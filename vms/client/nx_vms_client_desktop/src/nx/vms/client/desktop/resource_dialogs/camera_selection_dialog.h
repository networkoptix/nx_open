#pragma once

#include <nx/utils/uuid.h>
#include <core/resource/resource.h>
#include <ui/dialogs/common/session_aware_dialog.h>

#include <nx/vms/client/desktop/node_view/details/node_view_fwd.h>

namespace Ui { class CameraSelectionDialog; }

namespace nx::vms::client::desktop {


class CameraSelectionDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;
public:

    using ValidResourceCheck = std::function<bool (const QnResourcePtr& resource)>;
    using GetText = std::function<QString (const QnResourceList &resources, bool detailed)>;

    struct DummyPolicy
    {
        using resource_type = QnResource;
        static bool isResourceValid(const QnResourcePtr&) { return true; }
        static QString getText(const QnResourceList&, bool = true) { return QString(); }
        static inline bool emptyListIsValid() { return true; }
        static bool multiChoiceListIsValid() { return true; }
    };

    template<typename ResourcePolicy>
    static bool selectCameras(QnUuidSet& selectedCameras, QWidget* parent);

private:
    CameraSelectionDialog(
        const ValidResourceCheck& validResourceCheck,
        const GetText& getText,
        const QnUuidSet& selectedCameras,
        QWidget* parent);

    virtual ~CameraSelectionDialog() override;

    static bool selectCamerasInternal(
        const ValidResourceCheck& validResourceCheck,
        const GetText& getText,
        QnUuidSet& selectedCameras,
        QWidget* parent);

    void updateThumbnail(const QModelIndex& index);

private:
    struct Private;
    const QScopedPointer<Private> d;
    const QScopedPointer<Ui::CameraSelectionDialog> ui;
};

template<typename ResourcePolicy>
bool CameraSelectionDialog::selectCameras(
    QnUuidSet& selectedCameras,
    QWidget* parent)
{
    const auto validResourceCheck =
        [](const QnResourcePtr& resource)
        {
            const auto target = resource.dynamicCast<typename ResourcePolicy::resource_type>();
            return ResourcePolicy::isResourceValid(target);
        };

    const auto getText =
        [](const QnResourceList& resources, bool detailed)
        {
            const bool isValid = isResourcesListValid<ResourcePolicy>(resources);
            return isValid ? QString() : ResourcePolicy::getText(resources);
        };

    return selectCamerasInternal(validResourceCheck, getText, selectedCameras, parent);
}

} // namespace nx::vms::client::desktop
