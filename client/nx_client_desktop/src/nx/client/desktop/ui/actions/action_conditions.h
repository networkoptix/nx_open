#pragma once

#include <QtCore/QObject>
#include <QtCore/QSet>

#include <common/common_globals.h>
#include <client/client_globals.h>

#include <core/resource/resource_fwd.h>
#include <core/resource_management/resource_criterion.h>
#include <core/ptz/ptz_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include <ui/actions/action_parameter_types.h>

#include <nx/client/desktop/ui/actions/action_fwd.h>
#include <nx/client/desktop/ui/actions/action_types.h>

class QnActionParameters;
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
class Condition: public QObject, public QnWorkbenchContextAware
{
public:
    /**
     * Constructor.
     *
     * \param parent                    Context-aware parent.
     */
    Condition(QObject* parent);

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
    virtual ActionVisibility check(const QnActionParameters& parameters);

    /**
     * Specialized condition function that catches all action parameters that
     * are convertible to a resource list (<tt>Qn::ResourceType</tt>).
     */
    virtual ActionVisibility check(const QnResourceList& resources);

    /**
     * Specialized condition function that catches all action parameters that
     * are convertible to a list of layout items (<tt>Qn::LayoutItemType</tt>).
     */
    virtual ActionVisibility check(const QnLayoutItemIndexList& layoutItems);

    /**
     * Specialized condition function that catches all action parameters that
     * are convertible to a list of resource widgets. (<tt>Qn::WidgetType</tt>).
     */
    virtual ActionVisibility check(const QnResourceWidgetList& widgets);

    /**
     * Specialized condition function that catches all action parameters that
     * are convertible to a list of workbench layouts. (<tt>Qn::LayoutType</tt>).
     */
    virtual ActionVisibility check(const QnWorkbenchLayoutList& layouts);
};

ConditionPtr operator&&(const ConditionPtr& l, const ConditionPtr& r);
ConditionPtr operator||(const ConditionPtr& l, const ConditionPtr& r);
ConditionPtr operator!(const ConditionPtr& l);

/** Base condition class for actions that should be visible in videowall review mode only. */
class VideoWallReviewModeCondition: public Condition
{
public:
    VideoWallReviewModeCondition(QObject* parent);
    bool isVideoWallReviewMode() const;
    virtual ActionVisibility check(const QnActionParameters& parameters) override;
};

/** Condition class for actions that should be visible in layout tour review mode */
class LayoutTourReviewModeCondition: public Condition
{
public:
    LayoutTourReviewModeCondition(QObject* parent);
    virtual ActionVisibility check(const QnActionParameters& parameters) override;
};

/** Condition class for actions, that require owner privileges. */
class RequiresOwnerCondition: public Condition
{
public:
    RequiresOwnerCondition(QObject* parent);
    virtual ActionVisibility check(const QnActionParameters& parameters) override;
};

/**
 * Condition for a single resource widget that checks its zoomed state.
 */
class ItemZoomedCondition: public Condition
{
public:
    ItemZoomedCondition(bool requiredZoomedState, QObject* parent);
    virtual ActionVisibility check(const QnResourceWidgetList& widgets) override;

private:
    bool m_requiredZoomedState;
};

class SmartSearchCondition: public Condition
{
public:
    SmartSearchCondition(bool requiredGridDisplayValue, QObject* parent);
    SmartSearchCondition(QObject* parent);
    virtual ActionVisibility check(const QnResourceWidgetList& widgets) override;

private:
    bool m_hasRequiredGridDisplayValue;
    bool m_requiredGridDisplayValue;
};

class DisplayInfoCondition: public Condition
{
public:
    DisplayInfoCondition(bool requiredDisplayInfoValue, QObject* parent);
    DisplayInfoCondition(QObject* parent);
    virtual ActionVisibility check(const QnResourceWidgetList& widgets) override;

private:
    bool m_hasRequiredDisplayInfoValue;
    bool m_requiredDisplayInfoValue;
};

class ClearMotionSelectionCondition: public Condition
{
public:
    ClearMotionSelectionCondition(QObject* parent);
    virtual ActionVisibility check(const QnResourceWidgetList& widgets) override;
};

class CheckFileSignatureCondition: public Condition
{
public:
    CheckFileSignatureCondition(QObject* parent);
    virtual ActionVisibility check(const QnResourceWidgetList& widgets) override;
};

/**
 * ResourceCriterion-based action Condition.
 */
class ResourceCondition: public Condition
{
public:
    ResourceCondition(
        const QnResourceCriterion &criterion,
        MatchMode matchMode,
        QObject* parent);
    virtual ~ResourceCondition();
    virtual ActionVisibility check(const QnResourceList& resources) override;
    virtual ActionVisibility check(const QnResourceWidgetList& widgets) override;

protected:
    template<class Item, class ItemSequence>
    bool checkInternal(const ItemSequence &sequence);
    bool checkOne(const QnResourcePtr &resource);
    bool checkOne(QnResourceWidget *widget);

private:
    QnResourceCriterion m_criterion;
    MatchMode m_matchMode;
};


/**
 * Condition for resource removal.
 */
class ResourceRemovalCondition: public Condition
{
public:
    ResourceRemovalCondition(QObject* parent): Condition(parent) {}
    virtual ActionVisibility check(const QnActionParameters& parameters) override;
};

class StopSharingCondition: public Condition
{
public:
    StopSharingCondition(QObject* parent): Condition(parent) {}
    virtual ActionVisibility check(const QnActionParameters& parameters) override;
};

/**
 * Base class for edge-specific action conditions.
 */
class EdgeServerCondition: public Condition
{
public:
    EdgeServerCondition(bool isEdgeServer, QObject* parent);
    virtual ActionVisibility check(const QnResourceList& resources) override;

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
    RenameResourceCondition(QObject* parent);
    virtual ActionVisibility check(const QnActionParameters& parameters) override;
};

/**
 * Condition for removal of a layout item.
 */
class LayoutItemRemovalCondition: public Condition
{
public:
    LayoutItemRemovalCondition(QObject* parent);
    virtual ActionVisibility check(const QnLayoutItemIndexList& layoutItems) override;
};


/**
 * Condition for saving of a layout.
 */
class SaveLayoutCondition: public Condition
{
public:
    SaveLayoutCondition(bool isCurrent, QObject* parent);
    virtual ActionVisibility check(const QnResourceList& resources) override;

private:
    bool m_current;
};

/**
 * Condition for saving of a layout with a new name.
 */
class SaveLayoutAsCondition: public Condition
{
public:
    SaveLayoutAsCondition(bool isCurrent, QObject* parent);
    virtual ActionVisibility check(const QnResourceList& resources) override;

private:
    bool m_current;
};

/**
 * Condition based on the count of layouts that are currently open.
 */
class LayoutCountCondition: public Condition
{
public:
    LayoutCountCondition(int minimalRequiredCount, QObject* parent);
    virtual ActionVisibility check(const QnWorkbenchLayoutList& layouts) override;

private:
    int m_minimalRequiredCount;
};


/**
 * Condition for taking screenshot of a resource widget.
 */
class TakeScreenshotCondition: public Condition
{
public:
    TakeScreenshotCondition(QObject* parent);
    virtual ActionVisibility check(const QnResourceWidgetList& widgets) override;
};


/**
 * Condition for adjust video dialog of a resource widget.
 */
class AdjustVideoCondition: public Condition
{
public:
    AdjustVideoCondition(QObject* parent);
    virtual ActionVisibility check(const QnResourceWidgetList& widgets) override;
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
        ActionVisibility nonMatchingVisibility,
        QObject* parent);
    virtual ActionVisibility check(const QnActionParameters& parameters) override;

private:
    TimePeriodTypes m_periodTypes;
    ActionVisibility m_nonMatchingVisibility;
};

class ExportCondition: public Condition
{
public:
    ExportCondition(bool centralItemRequired, QObject* parent);
    virtual ActionVisibility check(const QnActionParameters& parameters) override;

private:
    bool m_centralItemRequired;
};

class AddBookmarkCondition: public Condition
{
public:
    AddBookmarkCondition(QObject* parent);
    virtual ActionVisibility check(const QnActionParameters& parameters) override;
};

class ModifyBookmarkCondition: public Condition
{
public:
    ModifyBookmarkCondition(QObject* parent);
    virtual ActionVisibility check(const QnActionParameters& parameters) override;
};

class RemoveBookmarksCondition: public Condition
{
public:
    RemoveBookmarksCondition(QObject* parent);
    virtual ActionVisibility check(const QnActionParameters& parameters) override;
};

class PreviewCondition: public ExportCondition
{
public:
    PreviewCondition(QObject* parent);
    virtual ActionVisibility check(const QnActionParameters& parameters) override;
};

class PanicCondition: public Condition
{
public:
    PanicCondition(QObject* parent);
    virtual ActionVisibility check(const QnActionParameters& parameters) override;
};

class ToggleTourCondition: public Condition
{
public:
    ToggleTourCondition(QObject* parent);
    virtual ActionVisibility check(const QnActionParameters& parameters) override;
};

class StartCurrentLayoutTourCondition: public Condition
{
public:
    StartCurrentLayoutTourCondition(QObject* parent);
    virtual ActionVisibility check(const QnActionParameters& parameters) override;
};

class ArchiveCondition: public Condition
{
public:
    ArchiveCondition(QObject* parent);
    virtual ActionVisibility check(const QnResourceList& resources) override;
};

class TimelineVisibleCondition: public Condition
{
public:
    TimelineVisibleCondition(QObject* parent);
    virtual ActionVisibility check(const QnActionParameters& parameters) override;
};

class ToggleTitleBarCondition: public Condition
{
public:
    ToggleTitleBarCondition(QObject* parent);
    virtual ActionVisibility check(const QnActionParameters& parameters) override;
};

class NoArchiveCondition: public Condition
{
public:
    NoArchiveCondition(QObject* parent);
    virtual ActionVisibility check(const QnActionParameters& parameters) override;
};

class OpenInFolderCondition: public Condition
{
public:
    OpenInFolderCondition(QObject* parent);
    virtual ActionVisibility check(const QnResourceList& resources) override;
    virtual ActionVisibility check(const QnLayoutItemIndexList& layoutItems) override;
};

class LayoutSettingsCondition: public Condition
{
public:
    LayoutSettingsCondition(QObject* parent);
    virtual ActionVisibility check(const QnResourceList& resources) override;
};

class CreateZoomWindowCondition: public Condition
{
public:
    CreateZoomWindowCondition(QObject* parent);
    virtual ActionVisibility check(const QnResourceWidgetList& widgets) override;
};

class TreeNodeTypeCondition: public Condition
{
public:
    TreeNodeTypeCondition(Qn::NodeType nodeType, QObject* parent);
    TreeNodeTypeCondition(QList<Qn::NodeType> nodeTypes, QObject* parent);

    virtual ActionVisibility check(const QnActionParameters& parameters) override;

private:
    QSet<Qn::NodeType> m_nodeTypes;
};

class ResourceStatusCondition: public Condition
{
public:
    ResourceStatusCondition(const QSet<Qn::ResourceStatus> statuses, bool allResources, QObject* parent);
    ResourceStatusCondition(Qn::ResourceStatus status, bool allResources, QObject* parent);
    virtual ActionVisibility check(const QnResourceList& resources) override;

private:
    QSet<Qn::ResourceStatus> m_statuses;
    bool m_all;
};

class NewUserLayoutCondition: public Condition
{
public:
    NewUserLayoutCondition(QObject* parent);
    virtual ActionVisibility check(const QnActionParameters& parameters) override;
};

class OpenInLayoutCondition: public Condition
{
public:
    OpenInLayoutCondition(QObject* parent);
    virtual ActionVisibility check(const QnActionParameters& parameters) override;

protected:
    bool canOpen(const QnResourceList& resources, const QnLayoutResourcePtr& layout) const;
};

class OpenInCurrentLayoutCondition: public OpenInLayoutCondition
{
public:
    OpenInCurrentLayoutCondition(QObject* parent);
    virtual ActionVisibility check(const QnActionParameters& parameters) override;
    virtual ActionVisibility check(const QnResourceList& resources) override;
};

class OpenInNewEntityCondition: public OpenInLayoutCondition
{
public:
    OpenInNewEntityCondition(QObject* parent);
    virtual ActionVisibility check(const QnActionParameters& parameters) override;
    virtual ActionVisibility check(const QnResourceList& resources) override;
    virtual ActionVisibility check(const QnLayoutItemIndexList& layoutItems) override;
};

class SetAsBackgroundCondition: public Condition
{
public:
    SetAsBackgroundCondition(QObject* parent);
    virtual ActionVisibility check(const QnResourceList& resources) override;
    virtual ActionVisibility check(const QnLayoutItemIndexList& layoutItems) override;
};

class ChangeResolutionCondition: public VideoWallReviewModeCondition
{
public:
    ChangeResolutionCondition(QObject* parent);
    virtual ActionVisibility check(const QnActionParameters& parameters) override;
};

/** Display action only if user is logged in. */
class LoggedInCondition: public Condition
{
public:
    LoggedInCondition(QObject* parent);
    virtual ActionVisibility check(const QnActionParameters& parameters) override;
};

class BrowseLocalFilesCondition: public Condition
{
public:
    BrowseLocalFilesCondition(QObject* parent);
    virtual ActionVisibility check(const QnActionParameters& parameters) override;
};

class PtzCondition: public Condition
{
public:
    PtzCondition(Qn::PtzCapabilities capabilities, bool disableIfPtzDialogVisible, QObject* parent);
    virtual ActionVisibility check(const QnActionParameters& parameters) override;
    virtual ActionVisibility check(const QnResourceList& resources) override;
    virtual ActionVisibility check(const QnResourceWidgetList& widgets) override;

private:
    bool check(const QnPtzControllerPtr &controller);

private:
    Qn::PtzCapabilities m_capabilities;
    bool m_disableIfPtzDialogVisible;
};

class NonEmptyVideowallCondition: public Condition
{
public:
    NonEmptyVideowallCondition(QObject* parent);
    virtual ActionVisibility check(const QnResourceList& resources) override;
};

class RunningVideowallCondition: public Condition
{
public:
    RunningVideowallCondition(QObject* parent);
    virtual ActionVisibility check(const QnResourceList& resources) override;
};

class SaveVideowallReviewCondition: public Condition
{
public:
    SaveVideowallReviewCondition(bool isCurrent, QObject* parent);
    virtual ActionVisibility check(const QnResourceList& resources) override;
private:
    bool m_current;
};

class StartVideowallCondition: public Condition
{
public:
    StartVideowallCondition(QObject* parent);
    virtual ActionVisibility check(const QnResourceList& resources) override;
};

class IdentifyVideoWallCondition: public RunningVideowallCondition
{
public:
    IdentifyVideoWallCondition(QObject* parent);
    virtual ActionVisibility check(const QnActionParameters& parameters) override;
};

class DetachFromVideoWallCondition: public Condition
{
public:
    DetachFromVideoWallCondition(QObject* parent);
    virtual ActionVisibility check(const QnActionParameters& parameters) override;
};

class StartVideoWallControlCondition: public Condition
{
public:
    StartVideoWallControlCondition(QObject* parent);
    virtual ActionVisibility check(const QnActionParameters& parameters) override;
};


class RotateItemCondition: public Condition
{
public:
    RotateItemCondition(QObject* parent);
    virtual ActionVisibility check(const QnResourceWidgetList& widgets) override;
};

class AutoStartAllowedCondition: public Condition
{
public:
    AutoStartAllowedCondition(QObject* parent);
    virtual ActionVisibility check(const QnActionParameters& parameters) override;
};

class DesktopCameraCondition: public Condition
{
public:
    DesktopCameraCondition(QObject* parent);
    virtual ActionVisibility check(const QnActionParameters& parameters) override;
};

class LightModeCondition: public Condition
{
public:
    LightModeCondition(Qn::LightModeFlags flags, QObject* parent);
    virtual ActionVisibility check(const QnActionParameters& parameters) override;

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

    ItemsCountCondition(int count, QObject* parent);
    virtual ActionVisibility check(const QnActionParameters& parameters) override;

private:
    int m_count;
};

class IoModuleCondition: public Condition
{
public:
    IoModuleCondition(QObject* parent);
    virtual ActionVisibility check(const QnResourceList& resources) override;
};

class MergeToCurrentSystemCondition: public Condition
{
public:
    MergeToCurrentSystemCondition(QObject* parent);
    virtual ActionVisibility check(const QnResourceList& resources) override;
};


class FakeServerCondition: public Condition
{
public:
    FakeServerCondition(bool allResources, QObject* parent);
    virtual ActionVisibility check(const QnResourceList& resources) override;

private:
    bool m_all;
};

class CloudServerCondition: public Condition
{
public:
    CloudServerCondition(MatchMode matchMode, QObject* parent);
    virtual ActionVisibility check(const QnResourceList& resources) override;

private:
    MatchMode m_matchMode;
};

namespace condition {

/** Visible in preview search mode only. */
ConditionPtr isPreviewSearchMode(QObject* parent);

/** Visible in safe mode only. */
ConditionPtr isSafeMode(QObject* parent);

/** Allowed only for resource parameters with corresponding flags. */
ConditionPtr hasFlags(Qn::ResourceFlags flags, MatchMode matchMode, QObject* parent);

} // namespace condition

} // namespace action
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
