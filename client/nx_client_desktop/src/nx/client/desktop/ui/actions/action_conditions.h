#pragma once

#include <QtCore/QObject>
#include <QtCore/QSet>

#include <common/common_globals.h>
#include <client/client_globals.h>

#include <core/resource/resource_fwd.h>
#include <core/ptz/ptz_fwd.h>
#include <core/ptz/ptz_constants.h>

#include <ui/workbench/workbench_context_aware.h>

#include <nx/client/desktop/ui/actions/action_fwd.h>
#include <nx/client/desktop/ui/actions/action_types.h>
#include <nx/client/desktop/ui/actions/action_parameter_types.h>

class QnWorkbenchContext;

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace action {

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
    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context);

    /**
     * Specialized condition function that catches all action parameters that
     * are convertible to a resource list (<tt>Qn::ResourceType</tt>).
     */
    virtual ActionVisibility check(const QnResourceList& resources, QnWorkbenchContext* context);

    /**
     * Specialized condition function that catches all action parameters that
     * are convertible to a list of layout items (<tt>Qn::LayoutItemType</tt>).
     */
    virtual ActionVisibility check(const QnLayoutItemIndexList& layoutItems, QnWorkbenchContext* context);

    /**
     * Specialized condition function that catches all action parameters that
     * are convertible to a list of resource widgets. (<tt>Qn::WidgetType</tt>).
     */
    virtual ActionVisibility check(const QnResourceWidgetList& widgets, QnWorkbenchContext* context);

    /**
     * Specialized condition function that catches all action parameters that
     * are convertible to a list of workbench layouts. (<tt>Qn::LayoutType</tt>).
     */
    virtual ActionVisibility check(const QnWorkbenchLayoutList& layouts, QnWorkbenchContext* context);
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


/** Base condition class for actions that should be visible in videowall review mode only. */
class VideoWallReviewModeCondition: public Condition
{
public:
    bool isVideoWallReviewMode(QnWorkbenchContext* context) const;
    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override;
};

/** Condition class for actions, that require owner privileges. */
class RequiresOwnerCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override;
};

/**
 * Condition for a single resource widget that checks its zoomed state.
 */
class ItemZoomedCondition: public Condition
{
public:
    ItemZoomedCondition(bool requiredZoomedState);
    virtual ActionVisibility check(const QnResourceWidgetList& widgets, QnWorkbenchContext* context) override;

private:
    bool m_requiredZoomedState;
};

class SmartSearchCondition: public Condition
{
public:
    SmartSearchCondition();
    SmartSearchCondition(bool requiredGridDisplayValue);
    virtual ActionVisibility check(const QnResourceWidgetList& widgets, QnWorkbenchContext* context) override;

private:
    bool m_hasRequiredGridDisplayValue;
    bool m_requiredGridDisplayValue;
};

class DisplayInfoCondition: public Condition
{
public:
    DisplayInfoCondition(bool requiredDisplayInfoValue);
    DisplayInfoCondition();
    virtual ActionVisibility check(const QnResourceWidgetList& widgets, QnWorkbenchContext* context) override;

private:
    bool m_hasRequiredDisplayInfoValue;
    bool m_requiredDisplayInfoValue;
};

class ClearMotionSelectionCondition: public Condition
{
public:
    virtual ActionVisibility check(const QnResourceWidgetList& widgets, QnWorkbenchContext* context) override;
};

/**
 * Condition for resource removal.
 */
class ResourceRemovalCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override;
};

class StopSharingCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override;
};

/**
 * Base class for edge-specific action conditions.
 */
class EdgeServerCondition: public Condition
{
public:
    EdgeServerCondition(bool isEdgeServer);
    virtual ActionVisibility check(const QnResourceList& resources, QnWorkbenchContext* context) override;

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
    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override;
};

/**
 * Condition for removal of a layout item.
 */
class LayoutItemRemovalCondition: public Condition
{
public:
    virtual ActionVisibility check(const QnLayoutItemIndexList& layoutItems, QnWorkbenchContext* context) override;
};


/**
 * Condition for saving of a layout.
 */
class SaveLayoutCondition: public Condition
{
public:
    SaveLayoutCondition(bool isCurrent);
    virtual ActionVisibility check(const QnResourceList& resources, QnWorkbenchContext* context) override;

private:
    bool m_current;
};

/**
 * Condition for saving of a layout with a new name.
 */
class SaveLayoutAsCondition: public Condition
{
public:
    SaveLayoutAsCondition(bool isCurrent);
    virtual ActionVisibility check(const QnResourceList& resources, QnWorkbenchContext* context) override;

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
    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override;

private:
    int m_minimalRequiredCount;
};


/**
 * Condition for taking screenshot of a resource widget.
 */
class TakeScreenshotCondition: public Condition
{
public:
    virtual ActionVisibility check(const QnResourceWidgetList& widgets, QnWorkbenchContext* context) override;
};


/**
 * Condition for adjust video dialog of a resource widget.
 */
class AdjustVideoCondition: public Condition
{
public:
    virtual ActionVisibility check(const QnResourceWidgetList& widgets, QnWorkbenchContext* context) override;
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
    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override;

private:
    TimePeriodTypes m_periodTypes;
    ActionVisibility m_nonMatchingVisibility;
};

class ExportCondition: public Condition
{
public:
    ExportCondition(bool centralItemRequired);
    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override;

private:
    bool m_centralItemRequired;
};

class AddBookmarkCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override;
};

class ModifyBookmarkCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override;
};

class RemoveBookmarksCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override;
};

class PreviewCondition: public ExportCondition
{
public:
    PreviewCondition();
    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override;
};

class PanicCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override;
};

class StartCurrentLayoutTourCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override;
};

class ArchiveCondition: public Condition
{
public:
    virtual ActionVisibility check(const QnResourceList& resources, QnWorkbenchContext* context) override;
};

class TimelineVisibleCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override;
};

class ToggleTitleBarCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override;
};

class NoArchiveCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override;
};

class OpenInFolderCondition: public Condition
{
public:
    virtual ActionVisibility check(const QnResourceList& resources, QnWorkbenchContext* context) override;
    virtual ActionVisibility check(const QnLayoutItemIndexList& layoutItems, QnWorkbenchContext* context) override;
};

class LayoutSettingsCondition: public Condition
{
public:
    virtual ActionVisibility check(const QnResourceList& resources, QnWorkbenchContext* context) override;
};

class CreateZoomWindowCondition: public Condition
{
public:
    virtual ActionVisibility check(const QnResourceWidgetList& widgets, QnWorkbenchContext* context) override;
};

class ResourceStatusCondition: public Condition
{
public:
    ResourceStatusCondition(const QSet<Qn::ResourceStatus> statuses, bool allResources);
    ResourceStatusCondition(Qn::ResourceStatus status, bool allResources);
    virtual ActionVisibility check(const QnResourceList& resources, QnWorkbenchContext* context) override;

private:
    QSet<Qn::ResourceStatus> m_statuses;
    bool m_all;
};

class NewUserLayoutCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override;
};

class OpenInLayoutCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override;

protected:
    bool canOpen(const QnResourceList& resources, const QnLayoutResourcePtr& layout) const;
};

class OpenInCurrentLayoutCondition: public OpenInLayoutCondition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override;
    virtual ActionVisibility check(const QnResourceList& resources, QnWorkbenchContext* context) override;
};

class OpenInNewEntityCondition: public OpenInLayoutCondition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override;
    virtual ActionVisibility check(const QnResourceList& resources, QnWorkbenchContext* context) override;
    virtual ActionVisibility check(const QnLayoutItemIndexList& layoutItems, QnWorkbenchContext* context) override;
};

class SetAsBackgroundCondition: public Condition
{
public:
    virtual ActionVisibility check(const QnResourceList& resources, QnWorkbenchContext* context) override;
    virtual ActionVisibility check(const QnLayoutItemIndexList& layoutItems, QnWorkbenchContext* context) override;
};

class ChangeResolutionCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters,
        QnWorkbenchContext* context) override;
};

class BrowseLocalFilesCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override;
};

class PtzCondition: public Condition
{
public:
    PtzCondition(Ptz::Capabilities capabilities, bool disableIfPtzDialogVisible);
    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override;
    virtual ActionVisibility check(const QnResourceList& resources, QnWorkbenchContext* context) override;
    virtual ActionVisibility check(const QnResourceWidgetList& widgets, QnWorkbenchContext* context) override;

private:
    bool check(const QnPtzControllerPtr &controller);

private:
    Ptz::Capabilities m_capabilities;
    bool m_disableIfPtzDialogVisible;
};

class NonEmptyVideowallCondition: public Condition
{
public:
    virtual ActionVisibility check(const QnResourceList& resources, QnWorkbenchContext* context) override;
};

class RunningVideowallCondition: public Condition
{
public:
    virtual ActionVisibility check(const QnResourceList& resources, QnWorkbenchContext* context) override;
};

class SaveVideowallReviewCondition: public Condition
{
public:
    SaveVideowallReviewCondition(bool isCurrent);
    virtual ActionVisibility check(const QnResourceList& resources, QnWorkbenchContext* context) override;
private:
    bool m_current;
};

class StartVideowallCondition: public Condition
{
public:
    virtual ActionVisibility check(const QnResourceList& resources, QnWorkbenchContext* context) override;
};

class IdentifyVideoWallCondition: public RunningVideowallCondition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override;
};

class DetachFromVideoWallCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override;
};

class StartVideoWallControlCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override;
};

class RotateItemCondition: public Condition
{
public:
    virtual ActionVisibility check(const QnResourceWidgetList& widgets, QnWorkbenchContext* context) override;
};

class AutoStartAllowedCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override;
};

class DesktopCameraCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override;
};

class LightModeCondition: public Condition
{
public:
    LightModeCondition(Qn::LightModeFlags flags);
    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override;

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
    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override;

private:
    int m_count;
};

class IoModuleCondition: public Condition
{
public:
    virtual ActionVisibility check(const QnResourceList& resources, QnWorkbenchContext* context) override;
};

class MergeToCurrentSystemCondition: public Condition
{
public:
    virtual ActionVisibility check(const QnResourceList& resources, QnWorkbenchContext* context) override;
};


class FakeServerCondition: public Condition
{
public:
    FakeServerCondition(bool allResources);
    virtual ActionVisibility check(const QnResourceList& resources, QnWorkbenchContext* context) override;

private:
    bool m_all;
};

class CloudServerCondition: public Condition
{
public:
    CloudServerCondition(MatchMode matchMode);
    virtual ActionVisibility check(const QnResourceList& resources, QnWorkbenchContext* context) override;

private:
    MatchMode m_matchMode;
};

namespace condition {

/** Visible always. */
ConditionWrapper always();

/** Visible when value is true. */
ConditionWrapper isTrue(bool value);

/** Visible when user is logged in (or at least logging in). */
ConditionWrapper isLoggedIn();

/** Check a condition only in the given scope */
ConditionWrapper scoped(ActionScope scope, ConditionWrapper&& condition);

/** Visible in preview search mode only. */
ConditionWrapper isPreviewSearchMode();

/** Visible in safe mode only. */
ConditionWrapper isSafeMode();

/** Allowed only for resource parameters with corresponding flags. */
ConditionWrapper hasFlags(Qn::ResourceFlags flags, MatchMode matchMode);

ConditionWrapper treeNodeType(QSet<Qn::NodeType> types);
inline ConditionWrapper treeNodeType(Qn::NodeType type) { return treeNodeType({{type}}); }

/** Visible in layout tour preview mode only. */
ConditionWrapper isLayoutTourReviewMode();

/** Check that fisheye cameras can save position only when dewarping is enabled. */
ConditionWrapper canSavePtzPosition();

ConditionWrapper hasTimePeriod();

ConditionWrapper hasArgument(int key, int targetTypeId = -1);

template<class T>
ConditionWrapper hasArgumentOfType(int key)
{
    return hasArgument(key, qMetaTypeId<T>());
}

/** Check if the resource is Entropix camera. */
ConditionWrapper isEntropixCamera();

} // namespace condition

} // namespace action
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
