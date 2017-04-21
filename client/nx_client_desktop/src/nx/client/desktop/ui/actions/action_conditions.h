#pragma once

#include <QtCore/QObject>
#include <QtCore/QSet>

#include <common/common_globals.h>
#include <client/client_globals.h>

#include <core/resource/resource_fwd.h>
#include <core/resource_management/resource_criterion.h>
#include <core/ptz/ptz_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include <ui/actions/action_fwd.h>

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
    Condition(QObject *parent);

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
    virtual ActionVisibility check(const QnActionParameters &parameters);

    /**
     * Specialized condition function that catches all action parameters that
     * are convertible to a resource list (<tt>Qn::ResourceType</tt>).
     */
    virtual ActionVisibility check(const QnResourceList &resources);

    /**
     * Specialized condition function that catches all action parameters that
     * are convertible to a list of layout items (<tt>Qn::LayoutItemType</tt>).
     */
    virtual ActionVisibility check(const QnLayoutItemIndexList &layoutItems);

    /**
     * Specialized condition function that catches all action parameters that
     * are convertible to a list of resource widgets. (<tt>Qn::WidgetType</tt>).
     */
    virtual ActionVisibility check(const QnResourceWidgetList &widgets);

    /**
     * Specialized condition function that catches all action parameters that
     * are convertible to a list of workbench layouts. (<tt>Qn::LayoutType</tt>).
     */
    virtual ActionVisibility check(const QnWorkbenchLayoutList &layouts);
};

ConditionPtr operator&&(const ConditionPtr& l, const ConditionPtr& r);
ConditionPtr operator||(const ConditionPtr& l, const ConditionPtr& r);
ConditionPtr operator~(const ConditionPtr& l);

/** Base condition class for actions that should be visible in videowall review mode only. */
class QnVideoWallReviewModeCondition: public Condition
{
public:
    QnVideoWallReviewModeCondition(QObject* parent);
protected:
    bool isVideoWallReviewMode() const;
    virtual ActionVisibility check(const QnActionParameters &parameters) override;
};

/** Condition class for actions that should be visible in layout tour review mode */
class QnLayoutTourReviewModeCondition: public Condition
{
public:
    QnLayoutTourReviewModeCondition(QObject* parent);
protected:
    virtual ActionVisibility check(const QnActionParameters &parameters) override;
};

/** Base condition class for actions that should be hidden in preview search mode
 *  and visible in all other cases - or vise versa. */
class QnPreviewSearchModeCondition: public Condition
{
public:
    QnPreviewSearchModeCondition(bool hide, QObject* parent):
        Condition(parent),
        m_hide(hide)
    {
    }
protected:
    bool isPreviewSearchMode(const QnActionParameters &parameters) const;
    virtual ActionVisibility check(const QnActionParameters &parameters) override;
private:
    /** Flag that describes if action should be visible or hidden in videowall review mode. */
    bool m_hide;
};

/** Condition class for actions, that are forbidden in safe mode. */
class QnForbiddenInSafeModeCondition: public Condition
{
public:
    QnForbiddenInSafeModeCondition(QObject *parent):
        Condition(parent)
    {
    }
    virtual ActionVisibility check(const QnActionParameters &parameters) override;
};

/** Condition class for actions, that require owner privileges. */
class QnRequiresOwnerCondition: public Condition
{
public:
    QnRequiresOwnerCondition(QObject *parent):
        Condition(parent)
    {
    }
    virtual ActionVisibility check(const QnActionParameters &parameters) override;
};

/**
 * Condition for a single resource widget that checks its zoomed state.
 */
class QnItemZoomedActionCondition: public Condition
{
public:
    QnItemZoomedActionCondition(bool requiredZoomedState, QObject *parent):
        Condition(parent),
        m_requiredZoomedState(requiredZoomedState)
    {
    }

    virtual ActionVisibility check(const QnResourceWidgetList &widgets) override;

private:
    bool m_requiredZoomedState;
};


class QnSmartSearchActionCondition: public Condition
{
public:
    QnSmartSearchActionCondition(bool requiredGridDisplayValue, QObject *parent):
        Condition(parent),
        m_hasRequiredGridDisplayValue(true),
        m_requiredGridDisplayValue(requiredGridDisplayValue)
    {
    }

    QnSmartSearchActionCondition(QObject *parent):
        Condition(parent),
        m_hasRequiredGridDisplayValue(false),
        m_requiredGridDisplayValue(false)
    {
    }

    virtual ActionVisibility check(const QnResourceWidgetList &widgets) override;

private:
    bool m_hasRequiredGridDisplayValue;
    bool m_requiredGridDisplayValue;
};

class QnDisplayInfoActionCondition: public Condition
{
public:
    QnDisplayInfoActionCondition(bool requiredDisplayInfoValue, QObject *parent):
        Condition(parent),
        m_hasRequiredDisplayInfoValue(true),
        m_requiredDisplayInfoValue(requiredDisplayInfoValue)
    {
    }

    QnDisplayInfoActionCondition(QObject *parent):
        Condition(parent),
        m_hasRequiredDisplayInfoValue(false),
        m_requiredDisplayInfoValue(false)
    {
    }

    virtual ActionVisibility check(const QnResourceWidgetList &widgets) override;

private:
    bool m_hasRequiredDisplayInfoValue;
    bool m_requiredDisplayInfoValue;
};

class QnClearMotionSelectionActionCondition: public Condition
{
public:
    QnClearMotionSelectionActionCondition(QObject *parent): Condition(parent) {}

    virtual ActionVisibility check(const QnResourceWidgetList &widgets) override;
};

class QnCheckFileSignatureActionCondition: public Condition
{
public:
    QnCheckFileSignatureActionCondition(QObject *parent): Condition(parent) {}
    virtual ActionVisibility check(const QnResourceWidgetList &widgets) override;
};


/**
 * QnResourceCriterion-based action condition.
 */
class QnResourceActionCondition: public Condition
{
public:
    QnResourceActionCondition(
        const QnResourceCriterion &criterion,
        MatchMode matchMode,
        QObject *parent);

    virtual ~QnResourceActionCondition();

    virtual ActionVisibility check(const QnResourceList &resources) override;

    virtual ActionVisibility check(const QnResourceWidgetList &widgets) override;

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
class QnResourceRemovalActionCondition: public Condition
{
public:
    QnResourceRemovalActionCondition(QObject *parent): Condition(parent) {}

    virtual ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnStopSharingActionCondition: public Condition
{
public:
    QnStopSharingActionCondition(QObject* parent): Condition(parent) {}

    virtual ActionVisibility check(const QnActionParameters &parameters) override;
};


/**
 * Base class for edge-specific action conditions.
 */
class QnEdgeServerCondition: public Condition
{
public:
    QnEdgeServerCondition(bool isEdgeServer, QObject *parent = NULL):
        Condition(parent),
        m_isEdgeServer(isEdgeServer)
    {
    }

    virtual ActionVisibility check(const QnResourceList &resources) override;
private:
    /** If this flag is true action is visible for edge servers only,
     *  in the other case - action is hidden for edge servers.
     */
    bool m_isEdgeServer;
};

/**
 * Condition for resource rename.
 */
class QnRenameResourceActionCondition: public Condition
{
public:
    QnRenameResourceActionCondition(QObject *parent = NULL): Condition(parent) {}

    virtual ActionVisibility check(const QnActionParameters &parameters) override;
};

/**
 * Condition for removal of a layout item.
 */
class QnLayoutItemRemovalActionCondition: public Condition
{
public:
    QnLayoutItemRemovalActionCondition(QObject *parent): Condition(parent) {}

    virtual ActionVisibility check(const QnLayoutItemIndexList &layoutItems) override;
};


/**
 * Condition for saving of a layout.
 */
class QnSaveLayoutActionCondition: public Condition
{
public:
    QnSaveLayoutActionCondition(bool isCurrent, QObject *parent): Condition(parent), m_current(isCurrent) {}

    virtual ActionVisibility check(const QnResourceList &resources) override;

private:
    bool m_current;
};

/**
 * Condition for saving of a layout with a new name.
 */
class QnSaveLayoutAsActionCondition: public Condition
{
public:
    QnSaveLayoutAsActionCondition(bool isCurrent, QObject *parent): Condition(parent), m_current(isCurrent) {}

    virtual ActionVisibility check(const QnResourceList &resources) override;

private:
    bool m_current;
};

/**
 * Condition based on the count of layouts that are currently open.
 */
class QnLayoutCountActionCondition: public Condition
{
public:
    QnLayoutCountActionCondition(int minimalRequiredCount, QObject *parent): Condition(parent), m_minimalRequiredCount(minimalRequiredCount) {}

    virtual ActionVisibility check(const QnWorkbenchLayoutList &layouts) override;

private:
    int m_minimalRequiredCount;
};


/**
 * Condition for taking screenshot of a resource widget.
 */
class QnTakeScreenshotActionCondition: public Condition
{
public:
    QnTakeScreenshotActionCondition(QObject *parent): Condition(parent) {}

    virtual ActionVisibility check(const QnResourceWidgetList &widgets) override;
};


/**
 * Condition for adjust video dialog of a resource widget.
 */
class QnAdjustVideoActionCondition: public Condition
{
public:
    QnAdjustVideoActionCondition(QObject *parent): Condition(parent) {}

    virtual ActionVisibility check(const QnResourceWidgetList &widgets) override;
};

/**
 * Condition that is based on the type of the time period provided as one
 * of the arguments of the parameters pack.
 */
class QnTimePeriodActionCondition: public Condition
{
public:
    QnTimePeriodActionCondition(
        TimePeriodTypes periodTypes,
        ActionVisibility nonMatchingVisibility,
        QObject* parent)
        :
        Condition(parent),
        m_periodTypes(periodTypes),
        m_nonMatchingVisibility(nonMatchingVisibility)
    {
    }

    virtual ActionVisibility check(const QnActionParameters &parameters) override;

private:
    TimePeriodTypes m_periodTypes;
    ActionVisibility m_nonMatchingVisibility;
};

class QnExportActionCondition: public Condition
{
public:
    QnExportActionCondition(bool centralItemRequired, QObject *parent):
        Condition(parent),
        m_centralItemRequired(centralItemRequired)
    {
    }

    virtual ActionVisibility check(const QnActionParameters &parameters) override;

private:
    bool m_centralItemRequired;
};

class QnAddBookmarkActionCondition: public Condition
{
public:
    QnAddBookmarkActionCondition(QObject *parent):
        Condition(parent)
    {
    }
    virtual ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnModifyBookmarkActionCondition: public Condition
{
public:
    QnModifyBookmarkActionCondition(QObject *parent):
        Condition(parent)
    {
    }
    virtual ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnRemoveBookmarksActionCondition: public Condition
{
public:
    QnRemoveBookmarksActionCondition(QObject *parent):
        Condition(parent)
    {
    }
    virtual ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnPreviewActionCondition: public QnExportActionCondition
{
public:
    QnPreviewActionCondition(QObject *parent): QnExportActionCondition(true, parent) {}

    virtual ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnPanicActionCondition: public Condition
{
public:
    QnPanicActionCondition(QObject *parent):
        Condition(parent)
    {
    }

    virtual ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnToggleTourActionCondition: public Condition
{
public:
    QnToggleTourActionCondition(QObject *parent): Condition(parent) {}
    virtual ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnStartCurrentLayoutTourActionCondition: public Condition
{
public:
    QnStartCurrentLayoutTourActionCondition(QObject *parent): Condition(parent) {}
    virtual ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnArchiveActionCondition: public Condition
{
public:
    QnArchiveActionCondition(QObject *parent): Condition(parent) {}

    virtual ActionVisibility check(const QnResourceList &resources) override;
};

class QnTimelineVisibleActionCondition: public Condition
{
public:
    QnTimelineVisibleActionCondition(QObject* parent): Condition(parent) {}
    virtual ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnToggleTitleBarActionCondition: public Condition
{
public:
    QnToggleTitleBarActionCondition(QObject *parent): Condition(parent) {}

    virtual ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnNoArchiveActionCondition: public Condition
{
public:
    QnNoArchiveActionCondition(QObject *parent): Condition(parent) {}

    virtual ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnOpenInFolderActionCondition: public Condition
{
public:
    QnOpenInFolderActionCondition(QObject *parent): Condition(parent) {}

    virtual ActionVisibility check(const QnResourceList &resources) override;

    virtual ActionVisibility check(const QnLayoutItemIndexList &layoutItems) override;
};

class QnLayoutSettingsActionCondition: public Condition
{
public:
    QnLayoutSettingsActionCondition(QObject *parent): Condition(parent) {}

    virtual ActionVisibility check(const QnResourceList &resources) override;
};

class QnCreateZoomWindowActionCondition: public Condition
{
public:
    QnCreateZoomWindowActionCondition(QObject *parent): Condition(parent) {}

    virtual ActionVisibility check(const QnResourceWidgetList &widgets) override;
};

class QnTreeNodeTypeCondition: public Condition
{
public:
    QnTreeNodeTypeCondition(Qn::NodeType nodeType, QObject *parent);
    QnTreeNodeTypeCondition(QList<Qn::NodeType> nodeTypes, QObject *parent);

    virtual ActionVisibility check(const QnActionParameters &parameters) override;

private:
    QSet<Qn::NodeType> m_nodeTypes;
};

class QnResourceStatusActionCondition: public Condition
{
public:
    QnResourceStatusActionCondition(const QSet<Qn::ResourceStatus> statuses, bool allResources, QObject *parent):
        Condition(parent), m_statuses(statuses), m_all(allResources)
    {
    }

    QnResourceStatusActionCondition(Qn::ResourceStatus status, bool allResources, QObject *parent):
        Condition(parent), m_statuses(QSet<Qn::ResourceStatus>() << status), m_all(allResources)
    {
    }

    virtual ActionVisibility check(const QnResourceList &resources) override;

private:
    QSet<Qn::ResourceStatus> m_statuses;
    bool m_all;
};

class QnNewUserLayoutActionCondition: public Condition
{
public:
    QnNewUserLayoutActionCondition(QObject* parent): Condition(parent) {}
    virtual ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnOpenInLayoutActionCondition: public Condition
{
public:
    QnOpenInLayoutActionCondition(QObject *parent): Condition(parent) {}
    virtual ActionVisibility check(const QnActionParameters &parameters) override;

protected:
    bool canOpen(const QnResourceList &resources, const QnLayoutResourcePtr& layout) const;
};

class QnOpenInCurrentLayoutActionCondition: public QnOpenInLayoutActionCondition
{
public:
    QnOpenInCurrentLayoutActionCondition(QObject *parent): QnOpenInLayoutActionCondition(parent) {}

    virtual ActionVisibility check(const QnActionParameters &parameters) override;
    virtual ActionVisibility check(const QnResourceList &resources) override;
};

class QnOpenInNewEntityActionCondition: public QnOpenInLayoutActionCondition
{
public:
    QnOpenInNewEntityActionCondition(QObject *parent): QnOpenInLayoutActionCondition(parent) {}

    virtual ActionVisibility check(const QnActionParameters &parameters) override;
    virtual ActionVisibility check(const QnResourceList &resources) override;
    virtual ActionVisibility check(const QnLayoutItemIndexList &layoutItems) override;
};

class QnSetAsBackgroundActionCondition: public Condition
{
public:
    QnSetAsBackgroundActionCondition(QObject *parent): Condition(parent) {}

    virtual ActionVisibility check(const QnResourceList &resources) override;
    virtual ActionVisibility check(const QnLayoutItemIndexList &layoutItems) override;
};

class QnChangeResolutionActionCondition: public QnVideoWallReviewModeCondition
{
public:
    QnChangeResolutionActionCondition(QObject* parent);
    virtual ActionVisibility check(const QnActionParameters &parameters) override;
};

/** Display action only if user is logged in. */
class QnLoggedInCondition: public Condition
{
public:
    QnLoggedInCondition(QObject* parent): Condition(parent) {}
    virtual ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnBrowseLocalFilesCondition: public Condition
{
public:
    QnBrowseLocalFilesCondition(QObject* parent);

    virtual ActionVisibility check(const QnActionParameters& parameters) override;
};

class QnPtzActionCondition: public Condition
{
public:
    QnPtzActionCondition(Qn::PtzCapabilities capabilities, bool disableIfPtzDialogVisible, QObject* parent):
        Condition(parent),
        m_capabilities(capabilities),
        m_disableIfPtzDialogVisible(disableIfPtzDialogVisible)
    {
    }

    virtual ActionVisibility check(const QnActionParameters &parameters) override;
    virtual ActionVisibility check(const QnResourceList &resources) override;
    virtual ActionVisibility check(const QnResourceWidgetList &widgets) override;

private:
    bool check(const QnPtzControllerPtr &controller);

private:
    Qn::PtzCapabilities m_capabilities;
    bool m_disableIfPtzDialogVisible;
};

class QnNonEmptyVideowallActionCondition: public Condition
{
public:
    QnNonEmptyVideowallActionCondition(QObject* parent): Condition(parent) {}
    virtual ActionVisibility check(const QnResourceList &resources) override;
};

class QnRunningVideowallActionCondition: public Condition
{
public:
    QnRunningVideowallActionCondition(QObject* parent): Condition(parent) {}
    virtual ActionVisibility check(const QnResourceList &resources) override;
};

class QnSaveVideowallReviewActionCondition: public Condition
{
public:
    QnSaveVideowallReviewActionCondition(bool isCurrent, QObject* parent): Condition(parent), m_current(isCurrent) {}
    virtual ActionVisibility check(const QnResourceList &resources) override;
private:
    bool m_current;
};

class QnStartVideowallActionCondition: public Condition
{
public:
    QnStartVideowallActionCondition(QObject* parent): Condition(parent) {}
    virtual ActionVisibility check(const QnResourceList &resources) override;
};

class QnIdentifyVideoWallActionCondition: public QnRunningVideowallActionCondition
{
public:
    QnIdentifyVideoWallActionCondition(QObject* parent): QnRunningVideowallActionCondition(parent) {}
    virtual ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnDetachFromVideoWallActionCondition: public Condition
{
public:
    QnDetachFromVideoWallActionCondition(QObject* parent): Condition(parent) {}
    virtual ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnStartVideoWallControlActionCondition: public Condition
{
public:
    QnStartVideoWallControlActionCondition(QObject* parent): Condition(parent) {}
    virtual ActionVisibility check(const QnActionParameters &parameters) override;
};


class QnRotateItemCondition: public Condition
{
public:
    QnRotateItemCondition(QObject* parent): Condition(parent) {}
    virtual ActionVisibility check(const QnResourceWidgetList &widgets) override;
};

class QnAutoStartAllowedActionCodition: public Condition
{
public:
    QnAutoStartAllowedActionCodition(QObject *parent): Condition(parent) {}
    virtual ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnDesktopCameraActionCondition: public Condition
{
public:
    QnDesktopCameraActionCondition(QObject *parent): Condition(parent) {}
    virtual ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnLightModeCondition: public Condition
{
public:
    QnLightModeCondition(Qn::LightModeFlags flags, QObject *parent = NULL):
        Condition(parent),
        m_lightModeFlags(flags)
    {
    }

    virtual ActionVisibility check(const QnActionParameters &parameters) override;

private:
    Qn::LightModeFlags m_lightModeFlags;
};

class QnItemsCountActionCondition: public Condition
{
public:
    enum Count
    {
        MultipleItems = -1,
        NoItems = 0,
        OneItem = 1
    };

    QnItemsCountActionCondition(int count, QObject *parent = NULL):
        Condition(parent),
        m_count(count)
    {
    }

    virtual ActionVisibility check(const QnActionParameters &parameters) override;

private:
    int m_count;
};

class QnIoModuleActionCondition: public Condition
{
public:
    QnIoModuleActionCondition(QObject *parent = NULL): Condition(parent) {}
    virtual ActionVisibility check(const QnResourceList &resources) override;
};

class QnMergeToCurrentSystemActionCondition: public Condition
{
public:
    QnMergeToCurrentSystemActionCondition(QObject *parent = NULL): Condition(parent) {}

    virtual ActionVisibility check(const QnResourceList &resources) override;
};


class QnFakeServerActionCondition: public Condition
{
public:
    QnFakeServerActionCondition(bool allResources = false, QObject *parent = NULL):
        Condition(parent),
        m_all(allResources)
    {
    }

    virtual ActionVisibility check(const QnResourceList &resources) override;

private:
    bool m_all;
};

class QnCloudServerActionCondition: public Condition
{
public:
    QnCloudServerActionCondition(MatchMode matchMode, QObject* parent = nullptr);

    virtual ActionVisibility check(const QnResourceList& resources) override;
private:
    MatchMode m_matchMode;
};

} // namespace action
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
