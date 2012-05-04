#ifndef QN_ACTION_CONDITIONS_H
#define QN_ACTION_CONDITIONS_H

#include <QObject>
#include <QWeakPointer>
#include <core/resource/resource_fwd.h>
#include <core/resourcemanagment/resource_criterion.h>
#include <ui/workbench/workbench_context_aware.h>
#include "action_fwd.h"
#include "actions.h"

namespace Qn {
    enum MatchMode {
        Any,        /**< Match if at least one resource satisfies the criterion. */
        All,        /**< Match only if all resources satisfy the criterion. */
        ExactlyOne  /**< Match only if exactly one resource satisfies condition. */
    };

    enum PeriodType {
        NullPeriod,     /**< No period. */
        EmptyPeriod,    /**< Period of zero length. */
        NormalPeriod,   /**< Normal period with non-zero length. */
    };

} // namespace Qn


class QnActionParameters;
class QnWorkbenchContext;

class QnActionCondition: public QObject, public QnWorkbenchContextAware {
public:
    QnActionCondition(QObject *parent = NULL);

    virtual Qn::ActionVisibility check(const QnResourceList &resources);

    virtual Qn::ActionVisibility check(const QnLayoutItemIndexList &layoutItems);

    virtual Qn::ActionVisibility check(const QnResourceWidgetList &widgets);

    virtual Qn::ActionVisibility check(const QnWorkbenchLayoutList &layouts);

    virtual Qn::ActionVisibility check(const QnActionParameters &parameters);
};


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

class QnMotionGridDisplayActionCondition: public QnActionCondition {
public:
    QnMotionGridDisplayActionCondition(bool requiredGridDisplayValue, QObject *parent = NULL): 
        QnActionCondition(parent),
        m_hasRequiredGridDisplayValue(true),
        m_requiredGridDisplayValue(requiredGridDisplayValue)
    {}

    QnMotionGridDisplayActionCondition(QObject *parent = NULL):
        QnActionCondition(parent),
        m_hasRequiredGridDisplayValue(false),
        m_requiredGridDisplayValue(false)
    {}

    virtual Qn::ActionVisibility check(const QnResourceWidgetList &widgets) override;

private:
    bool m_hasRequiredGridDisplayValue;
    bool m_requiredGridDisplayValue;
};

class QnCheckFileSignatureActionCondition: public QnActionCondition {
public:
    QnCheckFileSignatureActionCondition(QObject *parent = NULL): QnActionCondition(parent) {}
    virtual Qn::ActionVisibility check(const QnResourceWidgetList &widgets) override;
};


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


class QnResourceRemovalActionCondition: public QnActionCondition {
public:
    QnResourceRemovalActionCondition(QObject *parent = NULL): QnActionCondition(parent) {}

    virtual Qn::ActionVisibility check(const QnResourceList &resources) override;
};


class QnLayoutItemRemovalActionCondition: public QnActionCondition {
public:
    QnLayoutItemRemovalActionCondition(QObject *parent = NULL): QnActionCondition(parent) {}

    virtual Qn::ActionVisibility check(const QnLayoutItemIndexList &layoutItems) override;
};


class QnSaveLayoutActionCondition: public QnActionCondition {
public:
    QnSaveLayoutActionCondition(bool isCurrent, QObject *parent = NULL): QnActionCondition(parent), m_current(isCurrent) {}

    virtual Qn::ActionVisibility check(const QnResourceList &resources) override;

private:
    bool m_current;
};


class QnLayoutCountActionCondition: public QnActionCondition {
public:
    QnLayoutCountActionCondition(int requiredCount, QObject *parent = NULL): QnActionCondition(parent), m_requiredCount(requiredCount) {}

    virtual Qn::ActionVisibility check(const QnWorkbenchLayoutList &layouts) override;

private:
    int m_requiredCount;
};


class QnTakeScreenshotActionCondition: public QnActionCondition {
public:
    QnTakeScreenshotActionCondition(QObject *parent = NULL): QnActionCondition(parent) {}

    virtual Qn::ActionVisibility check(const QnResourceWidgetList &widgets) override;
};


class QnTimePeriodActionCondition: public QnActionCondition {
public:
    QnTimePeriodActionCondition(Qn::PeriodType periodType, Qn::ActionVisibility nonMatchingVisibility, QObject *parent = NULL):
        QnActionCondition(parent),
        m_periodType(periodType),
        m_nonMatchingVisibility(nonMatchingVisibility)
    {}

    virtual Qn::ActionVisibility check(const QnActionParameters &parameters) override;

private:
    Qn::PeriodType m_periodType;
    Qn::ActionVisibility m_nonMatchingVisibility;
};



#endif // QN_ACTION_CONDITIONS_H
