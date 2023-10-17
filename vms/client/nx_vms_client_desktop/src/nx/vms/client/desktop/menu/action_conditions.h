// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QSet>

#include <client/client_globals.h>
#include <common/common_globals.h>
#include <core/ptz/ptz_constants.h>
#include <core/ptz/ptz_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>

#include "action_fwd.h"
#include "action_parameter_types.h"
#include "action_types.h"

namespace nx::vms::client::desktop {

class WindowContext;

namespace menu {

enum TimePeriodType
{
    NullTimePeriod = 0x1,  /**< No period. */
    EmptyTimePeriod = 0x2,  /**< Period of zero length. */
    NormalTimePeriod = 0x4,  /**< Normal period with non-zero length. */
};
Q_DECLARE_FLAGS(TimePeriodTypes, TimePeriodType)
Q_DECLARE_OPERATORS_FOR_FLAGS(TimePeriodTypes)

/**
 * Base class for implementing conditions that must be satisfied for the
 * action to be triggerable via hotkey or visible in the menu.
 */
class Condition
{
public:
    Condition();
    virtual ~Condition();
    /**
     * Main condition checking function.
     *
     * By default it forwards control to one of the specialized functions
     * based on the type of the default parameter. Note that these
     * specialized functions cannot access other parameters.
     *
     * \param parameters                Parameters to check.
     * \returns                         Check result.
     */
    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context);

    /**
     * Specialized condition function that catches all action parameters that
     * are convertible to a resource list (<tt>Qn::ResourceType</tt>).
     */
    virtual ActionVisibility check(const QnResourceList& resources, WindowContext* context);

    /**
     * Specialized condition function that catches all action parameters that
     * are convertible to a list of layout items (<tt>Qn::LayoutItemType</tt>).
     */
    virtual ActionVisibility check(const LayoutItemIndexList& layoutItems, WindowContext* context);

    /**
     * Specialized condition function that catches all action parameters that
     * are convertible to a list of resource widgets. (<tt>Qn::WidgetType</tt>).
     */
    virtual ActionVisibility check(const QnResourceWidgetList& widgets, WindowContext* context);

    /**
     * Specialized condition function that catches all action parameters that
     * are convertible to a list of workbench layouts. (<tt>Qn::LayoutType</tt>).
     */
    virtual ActionVisibility check(const QnWorkbenchLayoutList& layouts, WindowContext* context);
};

class ConditionWrapper
{
public:
    ConditionWrapper();
    /** Wrapper will take ownership of the condition. */
    ConditionWrapper(Condition* condition);
    Condition* operator->() const;
    explicit operator bool() const;
private:
    std::unique_ptr<Condition> m_condition;
};

ConditionWrapper operator&&(ConditionWrapper&& l, ConditionWrapper&& r);
ConditionWrapper operator||(ConditionWrapper&& l, ConditionWrapper&& r);
ConditionWrapper operator!(ConditionWrapper&& l);

class ResourceCondition: public Condition
{
public:
    using CheckDelegate = std::function<bool(const QnResourcePtr& resource)>;

    ResourceCondition(CheckDelegate delegate, MatchMode matchMode);

    ActionVisibility check(const QnResourceList& resources,
        WindowContext* /*context*/);

    ActionVisibility check(const QnResourceWidgetList& widgets,
        WindowContext* /*context*/);

private:
    CheckDelegate m_delegate;
    MatchMode m_matchMode;
};

class PreventWhenFullscreenTransition: public Condition
{
public:
    static ConditionWrapper condition();

    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context) override;
};

/** Base condition class for actions that should be visible in videowall review mode only. */
class VideoWallReviewModeCondition: public Condition
{
public:
    bool isVideoWallReviewMode(WindowContext* context) const;
    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context) override;
};

/** Condition class for actions, that require Administrator privileges. */
class RequiresAdministratorCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context) override;
};

/**
 * Condition for a single resource widget that checks its zoomed state.
 */
class ItemZoomedCondition: public Condition
{
public:
    ItemZoomedCondition(bool requiredZoomedState);
    virtual ActionVisibility check(
        const QnResourceWidgetList& widgets,
        WindowContext* context) override;

private:
    bool m_requiredZoomedState;
};

class SmartSearchCondition: public Condition
{
public:
    SmartSearchCondition();
    SmartSearchCondition(bool requiredGridDisplayValue);
    virtual ActionVisibility check(
        const QnResourceWidgetList& widgets,
        WindowContext* context) override;

private:
    bool m_hasRequiredGridDisplayValue;
    bool m_requiredGridDisplayValue;
};

class DisplayInfoCondition: public Condition
{
public:
    virtual ActionVisibility check(
        const QnResourceWidgetList& widgets, WindowContext* context) override;
};

class ClearMotionSelectionCondition: public Condition
{
public:
    virtual ActionVisibility check(const QnResourceWidgetList& widgets, WindowContext* context) override;
};

/**
 * Condition for resource removal.
 */
class ResourceRemovalCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context) override;
};

/**
 * Base class for edge-specific action conditions.
 */
class EdgeServerCondition: public Condition
{
public:
    EdgeServerCondition(bool isEdgeServer);
    virtual ActionVisibility check(const QnResourceList& resources, WindowContext* context) override;

private:
    /** If this flag is true action is visible for edge servers only,
     *  in the other case - action is hidden for edge servers.
     */
    bool m_isEdgeServer;
};

/**
 * Condition for resource rename.
 */
class RenameResourceCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context) override;
};

/**
 * Condition for removal of a layout item.
 */
class LayoutItemRemovalCondition: public Condition
{
public:
    virtual ActionVisibility check(
        const LayoutItemIndexList& layoutItems,
        WindowContext* context) override;
};


/**
 * Condition for saving of a layout.
 */
class SaveLayoutCondition: public Condition
{
public:
    SaveLayoutCondition(bool isCurrent);
    virtual ActionVisibility check(const QnResourceList& resources, WindowContext* context) override;

private:
    bool m_current;
};

/**
 * Condition based on the count of layouts that are currently open.
 */
class LayoutCountCondition: public Condition
{
public:
    LayoutCountCondition(int minimalRequiredCount);
    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context) override;

private:
    int m_minimalRequiredCount;
};


/**
 * Condition for taking screenshot of a resource widget.
 */
class TakeScreenshotCondition: public Condition
{
public:
    virtual ActionVisibility check(const QnResourceWidgetList& widgets, WindowContext* context) override;
};


/**
 * Condition for adjust video dialog of a resource widget.
 */
class AdjustVideoCondition: public Condition
{
public:
    virtual ActionVisibility check(const QnResourceWidgetList& widgets, WindowContext* context) override;
};

/**
 * Condition that is based on the type of the time period provided as one
 * of the arguments of the parameters pack.
 */
class TimePeriodCondition: public Condition
{
public:
    TimePeriodCondition(
        TimePeriodTypes periodTypes,
        ActionVisibility nonMatchingVisibility);
    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context) override;

private:
    TimePeriodTypes m_periodTypes;
    ActionVisibility m_nonMatchingVisibility;
};

class AddBookmarkCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context) override;
};

class ModifyBookmarkCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context) override;
};

class RemoveBookmarksCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context) override;
};

class PreviewCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context) override;
};

class StartCurrentShowreelCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context) override;
};

class ArchiveCondition: public Condition
{
public:
    virtual ActionVisibility check(const QnResourceList& resources, WindowContext* context) override;
};

class TimelineVisibleCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context) override;
};

class ToggleTitleBarCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context) override;
};

class NoArchiveCondition: public Condition
{
public:
    virtual ActionVisibility check(const QnResourceList& resources, WindowContext* context) override;
};

class OpenInFolderCondition: public Condition
{
public:
    virtual ActionVisibility check(const QnResourceList& resources, WindowContext* context) override;
    virtual ActionVisibility check(const LayoutItemIndexList& layoutItems, WindowContext* context) override;
};

class LayoutSettingsCondition: public Condition
{
public:
    virtual ActionVisibility check(const QnResourceList& resources, WindowContext* context) override;
};

class CreateZoomWindowCondition: public Condition
{
public:
    virtual ActionVisibility check(const QnResourceWidgetList& widgets, WindowContext* context) override;
};

class ResourceStatusCondition: public Condition
{
public:
    ResourceStatusCondition(const QSet<nx::vms::api::ResourceStatus> statuses, bool allResources);
    ResourceStatusCondition(nx::vms::api::ResourceStatus status, bool allResources);
    virtual ActionVisibility check(const QnResourceList& resources, WindowContext* context) override;

private:
    QSet<nx::vms::api::ResourceStatus> m_statuses;
    bool m_all;
};

class NewUserLayoutCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context) override;
};

class OpenInLayoutCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context) override;

protected:
    bool canOpen(const QnResourceList& resources, const QnLayoutResourcePtr& layout) const;
};

class OpenInCurrentLayoutCondition: public OpenInLayoutCondition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context) override;
    virtual ActionVisibility check(const QnResourceList& resources, WindowContext* context) override;
};

class OpenInNewEntityCondition: public OpenInLayoutCondition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context) override;
    virtual ActionVisibility check(const QnResourceList& resources, WindowContext* context) override;
    virtual ActionVisibility check(const LayoutItemIndexList& layoutItems, WindowContext* context) override;
};

class SetAsBackgroundCondition: public Condition
{
public:
    virtual ActionVisibility check(const QnResourceList& resources, WindowContext* context) override;
    virtual ActionVisibility check(const LayoutItemIndexList& layoutItems, WindowContext* context) override;
};

class ChangeResolutionCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters,
        WindowContext* context) override;
};

class PtzCondition: public Condition
{
public:
    PtzCondition(Ptz::Capabilities capabilities, bool disableIfPtzDialogVisible);
    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context) override;
    virtual ActionVisibility check(const QnResourceList& resources, WindowContext* context) override;
    virtual ActionVisibility check(const QnResourceWidgetList& widgets, WindowContext* context) override;

private:
    bool checkInternal(const QnPtzControllerPtr& controller);

private:
    Ptz::Capabilities m_capabilities;
    bool m_disableIfPtzDialogVisible;
};

class NonEmptyVideowallCondition: public Condition
{
public:
    virtual ActionVisibility check(const QnResourceList& resources, WindowContext* context) override;
};

class SaveVideowallReviewCondition: public Condition
{
public:
    SaveVideowallReviewCondition(bool isCurrent);
    virtual ActionVisibility check(const QnResourceList& resources, WindowContext* context) override;
private:
    bool m_current;
};

class StartVideowallCondition: public Condition
{
public:
    virtual ActionVisibility check(const QnResourceList& resources, WindowContext* context) override;
};

class IdentifyVideoWallCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context) override;
};

class DetachFromVideoWallCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context) override;
};

class StartVideoWallControlCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context) override;
};

class RotateItemCondition: public Condition
{
public:
    virtual ActionVisibility check(const QnResourceWidgetList& widgets, WindowContext* context) override;
};

class AutoStartAllowedCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context) override;
};

class DesktopCameraCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context) override;
};

class LightModeCondition: public Condition
{
public:
    LightModeCondition(Qn::LightModeFlags flags);
    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context) override;

private:
    Qn::LightModeFlags m_lightModeFlags;
};

class ItemsCountCondition: public Condition
{
public:
    enum Count
    {
        MultipleItems = -1,
        NoItems = 0,
        OneItem = 1
    };

    ItemsCountCondition(int count);
    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context) override;

private:
    int m_count;
};

class IoModuleCondition: public Condition
{
public:
    virtual ActionVisibility check(const QnResourceList& resources, WindowContext* context) override;
};

class MergeToCurrentSystemCondition: public Condition
{
public:
    virtual ActionVisibility check(const QnResourceList& resources, WindowContext* context) override;
};

class FakeServerCondition: public Condition
{
public:
    FakeServerCondition(bool allResources);
    virtual ActionVisibility check(const QnResourceList& resources, WindowContext* context) override;

private:
    bool m_all;
};

class CloudServerCondition: public Condition
{
public:
    CloudServerCondition(MatchMode matchMode);
    virtual ActionVisibility check(const QnResourceList& resources, WindowContext* context) override;

private:
    MatchMode m_matchMode;
};

class ReachableServerCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context) override;
};

class HideServersInTreeCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context) override;
};

class ToggleProxiedResourcesCondition: public Condition
{
public:
    virtual ActionVisibility check(
        const Parameters& parameters,
        WindowContext* context) override;
};

/**
 * Calculates visual state of "Replace Camera..." context menu action.
 * @return <tt>Enabled</tt> if parameters have camera, camera is offline, it's not a virtual
 *     camera, it's not a multisensor camera, it's not the one attached to an NVR, it's not an
 *     I/O module and camera's parent server is online. <tt>Disabled</tt> if all the same as above
 *     except camera's parent server is offline. <tt>Invisible</tt> otherwise.
 */
class ReplaceCameraCondition: public Condition
{
public:
    virtual ActionVisibility check(
        const Parameters& parameters, WindowContext* context) override;
};

//-------------------------------------------------------------------------------------------------
// Declarations of resource grouping related actions conditions.
//-------------------------------------------------------------------------------------------------

class CreateNewResourceTreeGroupCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context);
};

class AssignResourceTreeGroupIdCondition: public Condition
{
public:
    virtual ActionVisibility check(
        const QnResourceList& resources, WindowContext* context) override;
};

class MoveResourceTreeGroupIdCondition: public Condition {
public:
    virtual ActionVisibility check(
        const QnResourceList& resources, WindowContext* context) override;
};

class RenameResourceTreeGroupCondition: public Condition
{
public:
    virtual ActionVisibility check(
        const Parameters& parameters, WindowContext* context) override;
};

class RemoveResourceTreeGroupCondition: public Condition
{
public:
    virtual ActionVisibility check(
        const Parameters& parameters, WindowContext* context) override;
};

namespace condition {

/** Visible always. */
ConditionWrapper always();

/** Visible when value is true. */
ConditionWrapper isTrue(bool value);

/** Visible when user is logged in (or at least logging in). */
ConditionWrapper isLoggedIn();

/** Visible when user is logged in to Cloud. */
ConditionWrapper isLoggedInToCloud();

/** Check a condition only in the given scope */
ConditionWrapper scoped(ActionScope scope, ConditionWrapper&& condition);

/** Check if current layout fits the provided condition. */
ConditionWrapper applyToCurrentLayout(ConditionWrapper&& condition);

/** Check if current user has certain global permission. */
ConditionWrapper hasGlobalPermission(GlobalPermission permission);

/** Visible in preview search mode only. */
ConditionWrapper isPreviewSearchMode();

/** Allowed only for resource parameters with corresponding flags. */
ConditionWrapper hasFlags(Qn::ResourceFlags flags, MatchMode matchMode);
ConditionWrapper hasFlags(Qn::ResourceFlags includeFlags, Qn::ResourceFlags excludeFlags,
    MatchMode matchMode);

/** Target resources have the video stream. */
ConditionWrapper hasVideo(MatchMode matchMode, bool value = true);

ConditionWrapper treeNodeType(QSet<ResourceTree::NodeType> types);
inline ConditionWrapper treeNodeType(ResourceTree::NodeType type)
{
    return treeNodeType(QSet<ResourceTree::NodeType>{type});
}

/** Visible in Showreel preview mode only. */
ConditionWrapper isShowreelReviewMode();

/** Check that fisheye cameras can save position only when dewarping is enabled. */
ConditionWrapper canSavePtzPosition();

ConditionWrapper hasTimePeriod();

ConditionWrapper hasArgument(int key, int targetTypeId = -1);

template<class T>
ConditionWrapper hasArgumentOfType(int key)
{
    return hasArgument(key, qMetaTypeId<T>());
}

/** Check if the resource is Analytics Engine. */
ConditionWrapper isAnalyticsEngine();

/** Playback sync is forced. */
ConditionWrapper syncIsForced();

ConditionWrapper canExportLayout();

ConditionWrapper canExportBookmark();
ConditionWrapper canExportBookmarks();

ConditionWrapper isDeviceAccessRelevant(nx::vms::api::AccessRights requiredAccessRights);

/** Whether virtual camera upload is enabled. */
ConditionWrapper virtualCameraUploadEnabled();

/** Whether virtual camera upload can be cancelled. */
ConditionWrapper canCancelVirtualCameraUpload();

ConditionWrapper currentLayoutIsVideowallScreen();

ConditionWrapper canForgetPassword();

/** Whether showreel can be created using the given resources. */
ConditionWrapper canMakeShowreel();

ConditionWrapper isWorkbenchVisible();

/** Check if there is a saved Session State for the current system-user pair. */
ConditionWrapper hasSavedWindowsState();

/** Check if there are other opened windows with the same system-user pair. */
ConditionWrapper hasOtherWindowsInSession();

/** Check if there are other opened windows. */
ConditionWrapper hasOtherWindows();

/** Check if new Event Rules Engine is present in the system and enabled. */
ConditionWrapper hasNewEventRulesEngine();

/**
 * Check if current user is allowed to see servers in the resources tree such feature may be
 * restricted for users without power user permissions by the global setting)
 */
ConditionWrapper allowedToShowServersInResourceTree();

ConditionWrapper joystickConnected();

/**
 * Checks if the client uses non-prod (test/stage/etc) cloud host.
 * Used for showing warning for the customers.
 */
ConditionWrapper showBetaUpgradeWarning();

/** Checks whether the provided id belongs to a cloud system which requires user interaction. */
ConditionWrapper isCloudSystemConnectionUserInteractionRequired();

ConditionWrapper videowallIsRunning();

/** Checks if "Save Layout As..." action is applicable. */
ConditionWrapper canSaveLayoutAs();

/** Checks if the user has cameras with editable settings. */
ConditionWrapper userHasCamerasWithEditableSettings();

/**
 * Checks if all resources have the permissions.
 * If no resources are provided, the check fails.
 */
ConditionWrapper hasPermissionsForResources(Qn::Permissions permissions);

/**
 * Checks whether a resource is Web Page.
 */
ConditionWrapper isWebPage();

/**
 * Checks whether a resource is Integration.
 */
ConditionWrapper isIntegration();

/**
 * Check whether a resource is Web Page or Integration.
 */
ConditionWrapper isWebPageOrIntegration();

} // namespace condition

} // namespace menu
} // namespace nx::vms::client::desktop
