#pragma once

#include <QtCore/QObject>
#include <QtCore/QSet>

#include <common/common_globals.h>
#include <client/client_globals.h>

#include <core/resource/resource_fwd.h>
#include <core/resource_management/resource_criterion.h>
#include <core/ptz/ptz_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include "action_fwd.h"
#include "actions.h"

namespace Qn {

enum MatchMode
{
    Any,            /**< Match if at least one resource satisfies the criterion. */
    All,            /**< Match only if all resources satisfy the criterion. */
    ExactlyOne      /**< Match only if exactly one resource satisfies condition. */
};

enum TimePeriodType
{
    NullTimePeriod = 0x1,  /**< No period. */
    EmptyTimePeriod = 0x2,  /**< Period of zero length. */
    NormalTimePeriod = 0x4,  /**< Normal period with non-zero length. */
};
Q_DECLARE_FLAGS(TimePeriodTypes, TimePeriodType)
Q_DECLARE_OPERATORS_FOR_FLAGS(TimePeriodTypes)

} // namespace Qn


class QnActionParameters;
class QnWorkbenchContext;

/**
 * Base class for implementing conditions that must be satisfied for the
 * action to be triggerable via hotkey or visible in the menu.
 */
class QnActionCondition: public QObject, public QnWorkbenchContextAware
{
public:
    /**
     * Constructor.
     *
     * \param parent                    Context-aware parent.
     */
    QnActionCondition(QObject *parent);

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
    virtual Qn::ActionVisibility check(const QnActionParameters &parameters);

    /**
     * Specialized condition function that catches all action parameters that
     * are convertible to a resource list (<tt>Qn::ResourceType</tt>).
     */
    virtual Qn::ActionVisibility check(const QnResourceList &resources);

    /**
     * Specialized condition function that catches all action parameters that
     * are convertible to a list of layout items (<tt>Qn::LayoutItemType</tt>).
     */
    virtual Qn::ActionVisibility check(const QnLayoutItemIndexList &layoutItems);

    /**
     * Specialized condition function that catches all action parameters that
     * are convertible to a list of resource widgets. (<tt>Qn::WidgetType</tt>).
     */
    virtual Qn::ActionVisibility check(const QnResourceWidgetList &widgets);

    /**
     * Specialized condition function that catches all action parameters that
     * are convertible to a list of workbench layouts. (<tt>Qn::LayoutType</tt>).
     */
    virtual Qn::ActionVisibility check(const QnWorkbenchLayoutList &layouts);
};

using QnActionConditionPtr = QPointer<QnActionCondition>;
QnActionConditionPtr operator&&(const QnActionConditionPtr& l, const QnActionConditionPtr& r);
QnActionConditionPtr operator||(const QnActionConditionPtr& l, const QnActionConditionPtr& r);
QnActionConditionPtr operator~(const QnActionConditionPtr& l);

/** Base condition class for actions that should be visible in videowall review mode only. */
class QnVideoWallReviewModeCondition: public QnActionCondition
{
public:
    QnVideoWallReviewModeCondition(QObject* parent);
protected:
    bool isVideoWallReviewMode() const;
    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
};

/** Condition class for actions that should be visible in layout tour review mode */
class QnLayoutTourReviewModeCondition: public QnActionCondition
{
public:
    QnLayoutTourReviewModeCondition(QObject* parent);
protected:
    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
};

/** Base condition class for actions that should be hidden in preview search mode
 *  and visible in all other cases - or vise versa. */
class QnPreviewSearchModeCondition: public QnActionCondition
{
public:
    QnPreviewSearchModeCondition(bool hide, QObject* parent):
        QnActionCondition(parent),
        m_hide(hide)
    {
    }
protected:
    bool isPreviewSearchMode(const QnActionParameters &parameters) const;
    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
private:
    /** Flag that describes if action should be visible or hidden in videowall review mode. */
    bool m_hide;
};

/** Condition class for actions, that are forbidden in safe mode. */
class QnForbiddenInSafeModeCondition: public QnActionCondition
{
public:
    QnForbiddenInSafeModeCondition(QObject *parent):
        QnActionCondition(parent)
    {
    }
    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
};

/** Condition class for actions, that require owner privileges. */
class QnRequiresOwnerCondition: public QnActionCondition
{
public:
    QnRequiresOwnerCondition(QObject *parent):
        QnActionCondition(parent)
    {
    }
    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
};

/**
 * Condition for a single resource widget that checks its zoomed state.
 */
class QnItemZoomedActionCondition: public QnActionCondition
{
public:
    QnItemZoomedActionCondition(bool requiredZoomedState, QObject *parent):
        QnActionCondition(parent),
        m_requiredZoomedState(requiredZoomedState)
    {
    }

    virtual Qn::ActionVisibility check(const QnResourceWidgetList &widgets) override;

private:
    bool m_requiredZoomedState;
};


class QnSmartSearchActionCondition: public QnActionCondition
{
public:
    QnSmartSearchActionCondition(bool requiredGridDisplayValue, QObject *parent):
        QnActionCondition(parent),
        m_hasRequiredGridDisplayValue(true),
        m_requiredGridDisplayValue(requiredGridDisplayValue)
    {
    }

    QnSmartSearchActionCondition(QObject *parent):
        QnActionCondition(parent),
        m_hasRequiredGridDisplayValue(false),
        m_requiredGridDisplayValue(false)
    {
    }

    virtual Qn::ActionVisibility check(const QnResourceWidgetList &widgets) override;

private:
    bool m_hasRequiredGridDisplayValue;
    bool m_requiredGridDisplayValue;
};

class QnDisplayInfoActionCondition: public QnActionCondition
{
public:
    QnDisplayInfoActionCondition(bool requiredDisplayInfoValue, QObject *parent):
        QnActionCondition(parent),
        m_hasRequiredDisplayInfoValue(true),
        m_requiredDisplayInfoValue(requiredDisplayInfoValue)
    {
    }

    QnDisplayInfoActionCondition(QObject *parent):
        QnActionCondition(parent),
        m_hasRequiredDisplayInfoValue(false),
        m_requiredDisplayInfoValue(false)
    {
    }

    virtual Qn::ActionVisibility check(const QnResourceWidgetList &widgets) override;

private:
    bool m_hasRequiredDisplayInfoValue;
    bool m_requiredDisplayInfoValue;
};

class QnClearMotionSelectionActionCondition: public QnActionCondition
{
public:
    QnClearMotionSelectionActionCondition(QObject *parent): QnActionCondition(parent) {}

    virtual Qn::ActionVisibility check(const QnResourceWidgetList &widgets) override;
};

class QnCheckFileSignatureActionCondition: public QnActionCondition
{
public:
    QnCheckFileSignatureActionCondition(QObject *parent): QnActionCondition(parent) {}
    virtual Qn::ActionVisibility check(const QnResourceWidgetList &widgets) override;
};


/**
 * QnResourceCriterion-based action condition.
 */
class QnResourceActionCondition: public QnActionCondition
{
public:
    QnResourceActionCondition(const QnResourceCriterion &criterion, Qn::MatchMode matchMode, QObject *parent);

    virtual ~QnResourceActionCondition();

    virtual Qn::ActionVisibility check(const QnResourceList &resources) override;

    virtual Qn::ActionVisibility check(const QnResourceWidgetList &widgets) override;

protected:
    template<class Item, class ItemSequence>
    bool checkInternal(const ItemSequence &sequence);

    bool checkOne(const QnResourcePtr &resource);

    bool checkOne(QnResourceWidget *widget);

private:
    QnResourceCriterion m_criterion;
    Qn::MatchMode m_matchMode;
};


/**
 * Condition for resource removal.
 */
class QnResourceRemovalActionCondition: public QnActionCondition
{
public:
    QnResourceRemovalActionCondition(QObject *parent): QnActionCondition(parent) {}

    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnStopSharingActionCondition: public QnActionCondition
{
public:
    QnStopSharingActionCondition(QObject* parent): QnActionCondition(parent) {}

    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
};


/**
 * Base class for edge-specific action conditions.
 */
class QnEdgeServerCondition: public QnActionCondition
{
public:
    QnEdgeServerCondition(bool isEdgeServer, QObject *parent = NULL):
        QnActionCondition(parent),
        m_isEdgeServer(isEdgeServer)
    {
    }

    virtual Qn::ActionVisibility check(const QnResourceList &resources) override;
private:
    /** If this flag is true action is visible for edge servers only,
     *  in the other case - action is hidden for edge servers.
     */
    bool m_isEdgeServer;
};

/**
 * Condition for resource rename.
 */
class QnRenameResourceActionCondition: public QnActionCondition
{
public:
    QnRenameResourceActionCondition(QObject *parent = NULL): QnActionCondition(parent) {}

    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
};

/**
 * Condition for removal of a layout item.
 */
class QnLayoutItemRemovalActionCondition: public QnActionCondition
{
public:
    QnLayoutItemRemovalActionCondition(QObject *parent): QnActionCondition(parent) {}

    virtual Qn::ActionVisibility check(const QnLayoutItemIndexList &layoutItems) override;
};


/**
 * Condition for saving of a layout.
 */
class QnSaveLayoutActionCondition: public QnActionCondition
{
public:
    QnSaveLayoutActionCondition(bool isCurrent, QObject *parent): QnActionCondition(parent), m_current(isCurrent) {}

    virtual Qn::ActionVisibility check(const QnResourceList &resources) override;

private:
    bool m_current;
};

/**
 * Condition for saving of a layout with a new name.
 */
class QnSaveLayoutAsActionCondition: public QnActionCondition
{
public:
    QnSaveLayoutAsActionCondition(bool isCurrent, QObject *parent): QnActionCondition(parent), m_current(isCurrent) {}

    virtual Qn::ActionVisibility check(const QnResourceList &resources) override;

private:
    bool m_current;
};

/**
 * Condition based on the count of layouts that are currently open.
 */
class QnLayoutCountActionCondition: public QnActionCondition
{
public:
    QnLayoutCountActionCondition(int minimalRequiredCount, QObject *parent): QnActionCondition(parent), m_minimalRequiredCount(minimalRequiredCount) {}

    virtual Qn::ActionVisibility check(const QnWorkbenchLayoutList &layouts) override;

private:
    int m_minimalRequiredCount;
};


/**
 * Condition for taking screenshot of a resource widget.
 */
class QnTakeScreenshotActionCondition: public QnActionCondition
{
public:
    QnTakeScreenshotActionCondition(QObject *parent): QnActionCondition(parent) {}

    virtual Qn::ActionVisibility check(const QnResourceWidgetList &widgets) override;
};


/**
 * Condition for adjust video dialog of a resource widget.
 */
class QnAdjustVideoActionCondition: public QnActionCondition
{
public:
    QnAdjustVideoActionCondition(QObject *parent): QnActionCondition(parent) {}

    virtual Qn::ActionVisibility check(const QnResourceWidgetList &widgets) override;
};

/**
 * Condition that is based on the type of the time period provided as one
 * of the arguments of the parameters pack.
 */
class QnTimePeriodActionCondition: public QnActionCondition
{
public:
    QnTimePeriodActionCondition(
        Qn::TimePeriodTypes periodTypes,
        Qn::ActionVisibility nonMatchingVisibility,
        QObject* parent)
        :
        QnActionCondition(parent),
        m_periodTypes(periodTypes),
        m_nonMatchingVisibility(nonMatchingVisibility)
    {
    }

    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;

private:
    Qn::TimePeriodTypes m_periodTypes;
    Qn::ActionVisibility m_nonMatchingVisibility;
};

class QnExportActionCondition: public QnActionCondition
{
public:
    QnExportActionCondition(bool centralItemRequired, QObject *parent):
        QnActionCondition(parent),
        m_centralItemRequired(centralItemRequired)
    {
    }

    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;

private:
    bool m_centralItemRequired;
};

class QnAddBookmarkActionCondition: public QnActionCondition
{
public:
    QnAddBookmarkActionCondition(QObject *parent):
        QnActionCondition(parent)
    {
    }
    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnModifyBookmarkActionCondition: public QnActionCondition
{
public:
    QnModifyBookmarkActionCondition(QObject *parent):
        QnActionCondition(parent)
    {
    }
    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnRemoveBookmarksActionCondition: public QnActionCondition
{
public:
    QnRemoveBookmarksActionCondition(QObject *parent):
        QnActionCondition(parent)
    {
    }
    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnPreviewActionCondition: public QnExportActionCondition
{
public:
    QnPreviewActionCondition(QObject *parent): QnExportActionCondition(true, parent) {}

    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnPanicActionCondition: public QnActionCondition
{
public:
    QnPanicActionCondition(QObject *parent):
        QnActionCondition(parent)
    {
    }

    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnToggleTourActionCondition: public QnActionCondition
{
public:
    QnToggleTourActionCondition(QObject *parent): QnActionCondition(parent) {}
    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnStartCurrentLayoutTourActionCondition: public QnActionCondition
{
public:
    QnStartCurrentLayoutTourActionCondition(QObject *parent): QnActionCondition(parent) {}
    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnArchiveActionCondition: public QnActionCondition
{
public:
    QnArchiveActionCondition(QObject *parent): QnActionCondition(parent) {}

    virtual Qn::ActionVisibility check(const QnResourceList &resources) override;
};

class QnTimelineVisibleActionCondition: public QnActionCondition
{
public:
    QnTimelineVisibleActionCondition(QObject* parent): QnActionCondition(parent) {}
    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnToggleTitleBarActionCondition: public QnActionCondition
{
public:
    QnToggleTitleBarActionCondition(QObject *parent): QnActionCondition(parent) {}

    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnNoArchiveActionCondition: public QnActionCondition
{
public:
    QnNoArchiveActionCondition(QObject *parent): QnActionCondition(parent) {}

    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnOpenInFolderActionCondition: public QnActionCondition
{
public:
    QnOpenInFolderActionCondition(QObject *parent): QnActionCondition(parent) {}

    virtual Qn::ActionVisibility check(const QnResourceList &resources) override;

    virtual Qn::ActionVisibility check(const QnLayoutItemIndexList &layoutItems) override;
};

class QnLayoutSettingsActionCondition: public QnActionCondition
{
public:
    QnLayoutSettingsActionCondition(QObject *parent): QnActionCondition(parent) {}

    virtual Qn::ActionVisibility check(const QnResourceList &resources) override;
};

class QnCreateZoomWindowActionCondition: public QnActionCondition
{
public:
    QnCreateZoomWindowActionCondition(QObject *parent): QnActionCondition(parent) {}

    virtual Qn::ActionVisibility check(const QnResourceWidgetList &widgets) override;
};

class QnTreeNodeTypeCondition: public QnActionCondition
{
public:
    QnTreeNodeTypeCondition(Qn::NodeType nodeType, QObject *parent);
    QnTreeNodeTypeCondition(QList<Qn::NodeType> nodeTypes, QObject *parent);

    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;

private:
    QSet<Qn::NodeType> m_nodeTypes;
};

class QnResourceStatusActionCondition: public QnActionCondition
{
public:
    QnResourceStatusActionCondition(const QSet<Qn::ResourceStatus> statuses, bool allResources, QObject *parent):
        QnActionCondition(parent), m_statuses(statuses), m_all(allResources)
    {
    }

    QnResourceStatusActionCondition(Qn::ResourceStatus status, bool allResources, QObject *parent):
        QnActionCondition(parent), m_statuses(QSet<Qn::ResourceStatus>() << status), m_all(allResources)
    {
    }

    virtual Qn::ActionVisibility check(const QnResourceList &resources) override;

private:
    QSet<Qn::ResourceStatus> m_statuses;
    bool m_all;
};

class QnNewUserLayoutActionCondition: public QnActionCondition
{
public:
    QnNewUserLayoutActionCondition(QObject* parent): QnActionCondition(parent) {}
    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnOpenInLayoutActionCondition: public QnActionCondition
{
public:
    QnOpenInLayoutActionCondition(QObject *parent): QnActionCondition(parent) {}
    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;

protected:
    bool canOpen(const QnResourceList &resources, const QnLayoutResourcePtr& layout) const;
};

class QnOpenInCurrentLayoutActionCondition: public QnOpenInLayoutActionCondition
{
public:
    QnOpenInCurrentLayoutActionCondition(QObject *parent): QnOpenInLayoutActionCondition(parent) {}

    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
    virtual Qn::ActionVisibility check(const QnResourceList &resources) override;
};

class QnOpenInNewEntityActionCondition: public QnOpenInLayoutActionCondition
{
public:
    QnOpenInNewEntityActionCondition(QObject *parent): QnOpenInLayoutActionCondition(parent) {}

    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
    virtual Qn::ActionVisibility check(const QnResourceList &resources) override;
    virtual Qn::ActionVisibility check(const QnLayoutItemIndexList &layoutItems) override;
};

class QnSetAsBackgroundActionCondition: public QnActionCondition
{
public:
    QnSetAsBackgroundActionCondition(QObject *parent): QnActionCondition(parent) {}

    virtual Qn::ActionVisibility check(const QnResourceList &resources) override;
    virtual Qn::ActionVisibility check(const QnLayoutItemIndexList &layoutItems) override;
};

class QnChangeResolutionActionCondition: public QnVideoWallReviewModeCondition
{
public:
    QnChangeResolutionActionCondition(QObject* parent);
    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
};

/** Display action only if user is logged in. */
class QnLoggedInCondition: public QnActionCondition
{
public:
    QnLoggedInCondition(QObject* parent): QnActionCondition(parent) {}
    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnBrowseLocalFilesCondition: public QnActionCondition
{
public:
    QnBrowseLocalFilesCondition(QObject* parent);

    virtual Qn::ActionVisibility check(const QnActionParameters& parameters) override;
};

class QnPtzActionCondition: public QnActionCondition
{
public:
    QnPtzActionCondition(Qn::PtzCapabilities capabilities, bool disableIfPtzDialogVisible, QObject* parent):
        QnActionCondition(parent),
        m_capabilities(capabilities),
        m_disableIfPtzDialogVisible(disableIfPtzDialogVisible)
    {
    }

    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
    virtual Qn::ActionVisibility check(const QnResourceList &resources) override;
    virtual Qn::ActionVisibility check(const QnResourceWidgetList &widgets) override;

private:
    bool check(const QnPtzControllerPtr &controller);

private:
    Qn::PtzCapabilities m_capabilities;
    bool m_disableIfPtzDialogVisible;
};

class QnNonEmptyVideowallActionCondition: public QnActionCondition
{
public:
    QnNonEmptyVideowallActionCondition(QObject* parent): QnActionCondition(parent) {}
    virtual Qn::ActionVisibility check(const QnResourceList &resources) override;
};

class QnRunningVideowallActionCondition: public QnActionCondition
{
public:
    QnRunningVideowallActionCondition(QObject* parent): QnActionCondition(parent) {}
    virtual Qn::ActionVisibility check(const QnResourceList &resources) override;
};

class QnSaveVideowallReviewActionCondition: public QnActionCondition
{
public:
    QnSaveVideowallReviewActionCondition(bool isCurrent, QObject* parent): QnActionCondition(parent), m_current(isCurrent) {}
    virtual Qn::ActionVisibility check(const QnResourceList &resources) override;
private:
    bool m_current;
};

class QnStartVideowallActionCondition: public QnActionCondition
{
public:
    QnStartVideowallActionCondition(QObject* parent): QnActionCondition(parent) {}
    virtual Qn::ActionVisibility check(const QnResourceList &resources) override;
};

class QnIdentifyVideoWallActionCondition: public QnRunningVideowallActionCondition
{
public:
    QnIdentifyVideoWallActionCondition(QObject* parent): QnRunningVideowallActionCondition(parent) {}
    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnDetachFromVideoWallActionCondition: public QnActionCondition
{
public:
    QnDetachFromVideoWallActionCondition(QObject* parent): QnActionCondition(parent) {}
    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnStartVideoWallControlActionCondition: public QnActionCondition
{
public:
    QnStartVideoWallControlActionCondition(QObject* parent): QnActionCondition(parent) {}
    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
};


class QnRotateItemCondition: public QnActionCondition
{
public:
    QnRotateItemCondition(QObject* parent): QnActionCondition(parent) {}
    virtual Qn::ActionVisibility check(const QnResourceWidgetList &widgets) override;
};

class QnAutoStartAllowedActionCodition: public QnActionCondition
{
public:
    QnAutoStartAllowedActionCodition(QObject *parent): QnActionCondition(parent) {}
    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnDesktopCameraActionCondition: public QnActionCondition
{
public:
    QnDesktopCameraActionCondition(QObject *parent): QnActionCondition(parent) {}
    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnLightModeCondition: public QnActionCondition
{
public:
    QnLightModeCondition(Qn::LightModeFlags flags, QObject *parent = NULL):
        QnActionCondition(parent),
        m_lightModeFlags(flags)
    {
    }

    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;

private:
    Qn::LightModeFlags m_lightModeFlags;
};

class QnItemsCountActionCondition: public QnActionCondition
{
public:
    enum Count
    {
        MultipleItems = -1,
        NoItems = 0,
        OneItem = 1
    };

    QnItemsCountActionCondition(int count, QObject *parent = NULL):
        QnActionCondition(parent),
        m_count(count)
    {
    }

    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;

private:
    int m_count;
};

class QnIoModuleActionCondition: public QnActionCondition
{
public:
    QnIoModuleActionCondition(QObject *parent = NULL): QnActionCondition(parent) {}
    virtual Qn::ActionVisibility check(const QnResourceList &resources) override;
};

class QnMergeToCurrentSystemActionCondition: public QnActionCondition
{
public:
    QnMergeToCurrentSystemActionCondition(QObject *parent = NULL): QnActionCondition(parent) {}

    virtual Qn::ActionVisibility check(const QnResourceList &resources) override;
};


class QnFakeServerActionCondition: public QnActionCondition
{
public:
    QnFakeServerActionCondition(bool allResources = false, QObject *parent = NULL):
        QnActionCondition(parent),
        m_all(allResources)
    {
    }

    virtual Qn::ActionVisibility check(const QnResourceList &resources) override;

private:
    bool m_all;
};

class QnCloudServerActionCondition: public QnActionCondition
{
public:
    QnCloudServerActionCondition(Qn::MatchMode matchMode, QObject* parent = nullptr);

    virtual Qn::ActionVisibility check(const QnResourceList& resources) override;
private:
    Qn::MatchMode m_matchMode;
};
