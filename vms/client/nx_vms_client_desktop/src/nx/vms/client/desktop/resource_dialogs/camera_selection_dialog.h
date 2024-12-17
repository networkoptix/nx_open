// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <core/resource/resource.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/resource_dialogs/resource_selection_widget.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/system_context.h>
#include <ui/dialogs/common/session_aware_dialog.h>

namespace Ui { class CameraSelectionDialog; }

namespace nx::vms::client::desktop {

class ResourceSelectionWidget;

struct PinnedItemDescription
{
    QString displayText;
    QnResourceIconCache::Key iconKey;
};

class CameraSelectionDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    using ResourceFilter = std::function<bool(const QnResourcePtr& resource)>;
    using ResourceValidator = std::function<bool(const QnResourcePtr& resource)>;
    using AlertTextProvider =
        std::function<QString(const QSet<QnResourcePtr>& resources, bool pinnedItemSelected)>;

    /**
     * @param resourceFilter Unary predicate which defines subset of cameras and devices will be
     *     displayed in the dialog.
     * @param resourceValidator Unary predicate which defines whether resource is valid choice for
     *     selection or not.
     */
    CameraSelectionDialog(
        const ResourceFilter& resourceFilter,
        const ResourceValidator& resourceValidator,
        const AlertTextProvider& alertTextProvider,
        bool permissionsHandledByFilter,
        const std::optional<PinnedItemDescription>& pinnedItemDescription,
        QWidget* parent);
    virtual ~CameraSelectionDialog() override;

    ResourceSelectionWidget* resourceSelectionWidget() const;

    /**
     * @return True if 'All Cameras' switch which toggles representation mode between
     *     'all cameras' / 'only valid cameras' is visible.
     */
    bool allCamerasSwitchVisible() const;

    /**
     * Sets whether 'All Cameras' switch should be visible or not.
     */
    void setAllCamerasSwitchVisible(bool visible);

    /**
     * @return True if dialog can be accepted when selection contains invalid devices.
     */
    bool allowInvalidSelection() const;

    /**
     * Sets whether dialog can be accepted when selection contains invalid devices or not. Default
     *     value is true.
     */
    void setAllowInvalidSelection(bool value);

    /**
     * @return Considering current user, system settings and resource tree settings, whether
     *     servers should be displayed in the resource tree or not.
     */
    bool showServersInTree() const;

public:
    struct DummyPolicy
    {
        using resource_type = QnResource;
        static bool isResourceValid(SystemContext*, const QnResourcePtr&) { return true; }
        static QString getText(SystemContext*, const QnResourceList&) { return {}; }
        static bool emptyListIsValid() { return true; }
        static bool multiChoiceListIsValid() { return true; }
        static bool showRecordingIndicator() { return false; }
        static bool showAllCamerasSwitch() { return false; }
    };

    template<typename ResourcePolicy>
    static bool selectCameras(SystemContext* context, UuidSet& selectedCameras, QWidget* parent);

    template<typename ResourcePolicy>
    static bool selectCameras(
        SystemContext* context,
        UuidSet& selectedCameras,
        const ResourcePolicy& policy,
        QWidget* parent);

    template<typename ResourcePolicy>
    static bool selectCameras(
        SystemContext* context,
        UuidSet& selectedCameras,
        bool& sourceCameraSelected,
        QWidget* parent);

    template<typename ResourcePolicy>
    static bool selectCameras(
        SystemContext* context,
        UuidSet& selectedCameras,
        bool& sourceCameraSelected,
        const ResourcePolicy& policy,
        QWidget* parent);

    template<typename ResourcePolicy>
    static bool resourceValidator(
        SystemContext* context, const ResourcePolicy& policy, const QnResourcePtr& resource);
    template<typename ResourcePolicy>
    static QString alertTextProvider(
        SystemContext* context,
        const ResourcePolicy& policy,
        const QSet<QnResourcePtr>& resourcesSet,
        bool pinnedItemSelected);

protected:
    virtual void showEvent(QShowEvent* event) override;

private:
    void updateDialogControls();

    // Translatable string is processed incorrectly in the template method in header file.
    static QString noCamerasAvailableMessage();
    static QString sourceCamera();

private:
    const std::unique_ptr<Ui::CameraSelectionDialog> ui;
    ResourceSelectionWidget* m_resourceSelectionWidget;
    bool m_allowInvalidSelection = true;
};

template<typename ResourcePolicy>
bool CameraSelectionDialog::selectCameras(
    SystemContext* context,
    UuidSet& selectedCameras,
    QWidget* parent)
{
    ResourcePolicy policy;
    return selectCameras(context, selectedCameras, policy, parent);
}

template<typename ResourcePolicy>
bool CameraSelectionDialog::selectCameras(
    SystemContext* context,
    UuidSet& selectedCameras,
    const ResourcePolicy& policy,
    QWidget* parent)
{
    const auto validator = [context, &policy](const QnResourcePtr& resource)
        {
            return resourceValidator(context, policy, resource);
        };

    const auto alertProvider =
        [context, &policy](const QSet<QnResourcePtr>& resourcesSet, bool pinnedItemSelected)
        {
            return alertTextProvider(context, policy, resourcesSet, pinnedItemSelected);
        };

    CameraSelectionDialog dialog(ResourceFilter(), validator, alertProvider,
        /*permissionsHandledByFilter*/ false, /*pinnedItemDescription*/ {}, parent);
    dialog.resourceSelectionWidget()->setSelectedResourcesIds(selectedCameras);
    dialog.setAllCamerasSwitchVisible(ResourcePolicy::showAllCamerasSwitch());
    dialog.resourceSelectionWidget()->setShowRecordingIndicator(
        ResourcePolicy::showRecordingIndicator());

    if (!ResourcePolicy::multiChoiceListIsValid())
    {
        dialog.resourceSelectionWidget()->setSelectionMode(
            ResourceSelectionMode::ExclusiveSelection);
    }

    if (dialog.resourceSelectionWidget()->isEmpty())
    {
        // Translatable string is processed incorrectly in the template method in header file.
        QnMessageBox::warning(parent, noCamerasAvailableMessage());
        return false;
    }

    if (dialog.exec() != QDialog::Accepted)
        return false;

    selectedCameras = dialog.resourceSelectionWidget()->selectedResourcesIds();
    return true;
}

template<typename ResourcePolicy>
bool CameraSelectionDialog::selectCameras(
    SystemContext* context,
    UuidSet& selectedCameras,
    bool& sourceCameraSelected,
    QWidget* parent)
{
    ResourcePolicy policy;
    return selectCameras(
        context, selectedCameras, sourceCameraSelected, policy, parent);
}

template<typename ResourcePolicy>
bool CameraSelectionDialog::selectCameras(
    SystemContext* context,
    UuidSet& selectedCameras,
    bool& sourceCameraSelected,
    const ResourcePolicy& policy,
    QWidget* parent)
{
    const auto validator = [context, &policy](const QnResourcePtr& resource)
        {
            return resourceValidator(context, policy, resource);
        };

    const auto alertProvider =
        [context, &policy](const QSet<QnResourcePtr>& resourcesSet, bool pinnedItemSelected)
        {
            return alertTextProvider(context, policy, resourcesSet, pinnedItemSelected);
        };

    static const PinnedItemDescription kPinnedItemDescription{
        sourceCamera(), QnResourceIconCache::Camera};

    CameraSelectionDialog dialog(ResourceFilter{}, validator, alertProvider,
        /*permissionsHandledByFilter*/ false, kPinnedItemDescription, parent);

    dialog.setAllCamerasSwitchVisible(ResourcePolicy::showAllCamerasSwitch());
    dialog.resourceSelectionWidget()->setShowRecordingIndicator(
        ResourcePolicy::showRecordingIndicator());
    if (!ResourcePolicy::multiChoiceListIsValid())
    {
        dialog.resourceSelectionWidget()->setSelectionMode(
            ResourceSelectionMode::ExclusiveSelection);
    }

    // Must be called after selection mode is set.
    dialog.resourceSelectionWidget()->setSelectedResourcesIds(selectedCameras);
    dialog.resourceSelectionWidget()->setPinnedItemSelected(sourceCameraSelected);

    if (dialog.resourceSelectionWidget()->isEmpty())
    {
        // Translatable string is processed incorrectly in the template method in header file.
        QnMessageBox::warning(parent, noCamerasAvailableMessage());
        return false;
    }

    if (dialog.exec() != QDialog::Accepted)
        return false;

    selectedCameras = dialog.resourceSelectionWidget()->selectedResourcesIds();
    sourceCameraSelected = dialog.resourceSelectionWidget()->pinnedItemSelected();

    return true;
}

template<typename ResourcePolicy>
bool CameraSelectionDialog::resourceValidator(
    SystemContext* context, const ResourcePolicy& policy, const QnResourcePtr& resource)
{
    const auto target = resource.dynamicCast<typename ResourcePolicy::resource_type>();
    if (target.isNull())
        return true;

    return policy.isResourceValid(context, target);
}

template<typename ResourcePolicy>
static bool isResourcesListValid(
    SystemContext* context,
    const ResourcePolicy& policy,
    const QnResourceList& resources)
{
    typedef typename ResourcePolicy::resource_type ResourceType;

    auto filtered = resources.filtered<ResourceType>();

    if (filtered.isEmpty())
        return ResourcePolicy::emptyListIsValid();

    if (filtered.size() > 1 && !ResourcePolicy::multiChoiceListIsValid())
        return false;

    for (const auto& resource: filtered)
    {
        if (!policy.isResourceValid(context, resource))
            return false;
    }

    return true;
}

template<typename ResourcePolicy>
QString CameraSelectionDialog::alertTextProvider(
    SystemContext* context,
    const ResourcePolicy& policy,
    const QSet<QnResourcePtr>& resourcesSet,
    bool pinnedItemSelected)
{
    if (resourcesSet.isEmpty() && !policy.emptyListIsValid() && pinnedItemSelected)
        return {};

    QnResourceList resourcesList{resourcesSet.cbegin(), resourcesSet.cend()};

    const bool isValid = isResourcesListValid(context, policy, resourcesList);
    return isValid ? QString{} : policy.getText(context, resourcesList);
}

} // namespace nx::vms::client::desktop
