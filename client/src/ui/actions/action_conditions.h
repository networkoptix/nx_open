#ifndef QN_ACTION_CONDITIONS_H
#define QN_ACTION_CONDITIONS_H

#include <QtCore/QObject>
#include <QtCore/QWeakPointer>

#include <core/resource/resource_fwd.h>
#include <core/resource_managment/resource_criterion.h>

#include <recording/time_period.h>
#include <ui/workbench/workbench_context_aware.h>

#include "action_fwd.h"
#include "actions.h"

namespace Qn {
    enum MatchMode {
        Any,            /**< Match if at least one resource satisfies the criterion. */
        All,            /**< Match only if all resources satisfy the criterion. */
        ExactlyOne      /**< Match only if exactly one resource satisfies condition. */
    };

} // namespace Qn


class QnActionParameters;
class QnWorkbenchContext;

/**
 * Base class for implementing conditions that must be satisfied for the
 * action to be triggerable via hotkey or visible in the menu.
 */
class QnActionCondition: public QObject, public QnWorkbenchContextAware {
public:
    /**
     * Constructor.
     * 
     * \param parent                    Context-aware parent.
     */
    QnActionCondition(QObject *parent = NULL);

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


/**
 * Condition for a single resource widget that checks its zoomed state.
 */
class QnItemZoomedActionCondition: public QnActionCondition {
public:
    QnItemZoomedActionCondition(bool requiredZoomedState, QObject *parent = NULL):
        QnActionCondition(parent),
        m_requiredZoomedState(requiredZoomedState)
    {}

    virtual Qn::ActionVisibility check(const QnResourceWidgetList &widgets) override;
     
private:
    bool m_requiredZoomedState;
};


class QnSmartSearchActionCondition: public QnActionCondition {
public:
    QnSmartSearchActionCondition(bool requiredGridDisplayValue, QObject *parent = NULL): 
        QnActionCondition(parent),
        m_hasRequiredGridDisplayValue(true),
        m_requiredGridDisplayValue(requiredGridDisplayValue)
    {}

    QnSmartSearchActionCondition(QObject *parent = NULL):
        QnActionCondition(parent),
        m_hasRequiredGridDisplayValue(false),
        m_requiredGridDisplayValue(false)
    {}

    virtual Qn::ActionVisibility check(const QnResourceWidgetList &widgets) override;

private:
    bool m_hasRequiredGridDisplayValue;
    bool m_requiredGridDisplayValue;
};

class QnDisplayInfoActionCondition: public QnActionCondition {
public:
    QnDisplayInfoActionCondition(bool requiredDisplayInfoValue, QObject *parent = NULL):
        QnActionCondition(parent),
        m_hasRequiredDisplayInfoValue(true),
        m_requiredDisplayInfoValue(requiredDisplayInfoValue)
    {}

    QnDisplayInfoActionCondition(QObject *parent = NULL):
        QnActionCondition(parent),
        m_hasRequiredDisplayInfoValue(false),
        m_requiredDisplayInfoValue(false)
    {}

    virtual Qn::ActionVisibility check(const QnResourceWidgetList &widgets) override;

private:
    bool m_hasRequiredDisplayInfoValue;
    bool m_requiredDisplayInfoValue;
};

class QnClearMotionSelectionActionCondition: public QnActionCondition {
public:
    QnClearMotionSelectionActionCondition(QObject *parent = NULL): QnActionCondition(parent) {}

    virtual Qn::ActionVisibility check(const QnResourceWidgetList &widgets) override;
};

class QnCheckFileSignatureActionCondition: public QnActionCondition {
public:
    QnCheckFileSignatureActionCondition(QObject *parent = NULL): QnActionCondition(parent) {}
    virtual Qn::ActionVisibility check(const QnResourceWidgetList &widgets) override;
};


/**
 * QnResourceCriterion-based action condition.
 */
class QnResourceActionCondition: public QnActionCondition {
public:
    QnResourceActionCondition(const QnResourceCriterion &criterion, Qn::MatchMode matchMode = Qn::All, QObject *parent = NULL);

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
class QnResourceRemovalActionCondition: public QnActionCondition {
public:
    QnResourceRemovalActionCondition(QObject *parent = NULL): QnActionCondition(parent) {}

    virtual Qn::ActionVisibility check(const QnResourceList &resources) override;
};


/**
 * Condition for removal of a layout item.
 */
class QnLayoutItemRemovalActionCondition: public QnActionCondition {
public:
    QnLayoutItemRemovalActionCondition(QObject *parent = NULL): QnActionCondition(parent) {}

    virtual Qn::ActionVisibility check(const QnLayoutItemIndexList &layoutItems) override;
};


/**
 * Condition for saving of a layout.
 */
class QnSaveLayoutActionCondition: public QnActionCondition {
public:
    QnSaveLayoutActionCondition(bool isCurrent, QObject *parent = NULL): QnActionCondition(parent), m_current(isCurrent) {}

    virtual Qn::ActionVisibility check(const QnResourceList &resources) override;

private:
    bool m_current;
};


/**
 * Condition based on the count of layouts that are currently open.
 */
class QnLayoutCountActionCondition: public QnActionCondition {
public:
    QnLayoutCountActionCondition(int minimalRequiredCount, QObject *parent = NULL): QnActionCondition(parent), m_minimalRequiredCount(minimalRequiredCount) {}

    virtual Qn::ActionVisibility check(const QnWorkbenchLayoutList &layouts) override;

private:
    int m_minimalRequiredCount;
};


/**
 * Condition for taking screenshot of a resource widget.
 */
class QnTakeScreenshotActionCondition: public QnActionCondition {
public:
    QnTakeScreenshotActionCondition(QObject *parent = NULL): QnActionCondition(parent) {}

    virtual Qn::ActionVisibility check(const QnResourceWidgetList &widgets) override;
};


/**
 * Condition that is based on the type of the time period provided as one
 * of the arguments of the parameters pack.
 */
class QnTimePeriodActionCondition: public QnActionCondition {
public:
    QnTimePeriodActionCondition(Qn::TimePeriodTypes periodTypes, Qn::ActionVisibility nonMatchingVisibility, bool centralItemRequired, QObject *parent = NULL):
        QnActionCondition(parent),
        m_periodTypes(periodTypes),
        m_nonMatchingVisibility(nonMatchingVisibility),
        m_centralItemRequired(centralItemRequired)
    {}

    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;

private:
    Qn::TimePeriodTypes m_periodTypes;
    Qn::ActionVisibility m_nonMatchingVisibility;
    bool m_centralItemRequired;
};

class QnExportActionCondition: public QnActionCondition {
public:
    QnExportActionCondition(bool centralItemRequired, QObject *parent = NULL):
        QnActionCondition(parent),
        m_centralItemRequired(centralItemRequired)
    {}

    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;

private:
    bool m_centralItemRequired;
};

class QnPanicActionCondition: public QnActionCondition {
public:
    QnPanicActionCondition(QObject *parent = NULL):
        QnActionCondition(parent)
    {}

    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnToggleTourActionCondition: public QnActionCondition {
public:
    QnToggleTourActionCondition(QObject *parent = NULL): QnActionCondition(parent) {}

    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnArchiveActionCondition: public QnActionCondition {
public:
    QnArchiveActionCondition(QObject *parent = NULL): QnActionCondition(parent) {}

    virtual Qn::ActionVisibility check(const QnResourceList &resources) override;
};

class QnToggleTitleBarActionCondition: public QnActionCondition {
public:
    QnToggleTitleBarActionCondition(QObject *parent = NULL): QnActionCondition(parent) {}

    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnNoArchiveActionCondition: public QnActionCondition {
public:
    QnNoArchiveActionCondition(QObject *parent = NULL): QnActionCondition(parent) {}

    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnDisconnectActionCondition: public QnActionCondition {
public:
    QnDisconnectActionCondition(QObject *parent = NULL): QnActionCondition(parent) {}

    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;
};

class QnOpenInFolderActionCondition: public QnActionCondition {
public:
    QnOpenInFolderActionCondition(QObject *parent = NULL): QnActionCondition(parent) {}

    virtual Qn::ActionVisibility check(const QnResourceList &resources) override;
};

#endif // QN_ACTION_CONDITIONS_H
