#ifndef QN_ACTION_CONDITIONS_H
#define QN_ACTION_CONDITIONS_H

#include <QObject>
#include <QWeakPointer>
#include <core/resource/resource_fwd.h>
#include <core/resourcemanagment/resource_criterion.h>
#include <ui/workbench/workbench_context_aware.h>
#include "action_fwd.h"
#include "actions.h"

class QnWorkbenchContext;

class QnActionCondition: public QObject, public QnWorkbenchContextAware {
public:
    QnActionCondition(QObject *parent = NULL);

    virtual Qn::ActionVisibility check(const QnResourceList &resources);

    virtual Qn::ActionVisibility check(const QnLayoutItemIndexList &layoutItems);

    virtual Qn::ActionVisibility check(const QnResourceWidgetList &widgets);

    virtual Qn::ActionVisibility check(const QnWorkbenchLayoutList &layouts);

    virtual Qn::ActionVisibility check(const QVariant &items);
};


class QnTargetlessActionCondition: public QnActionCondition {
    typedef QnActionCondition base_type;

public:
    QnTargetlessActionCondition(QObject *parent = NULL): QnActionCondition(parent) {}

    using base_type::check;

    virtual Qn::ActionVisibility check(const QVariant &items) override;
};


class QnMotionGridDisplayActionCondition: public QnActionCondition {
public:
    /**
     * Condition to check. 
     */
    enum Condition {
        HasShownGrid,
        HasHiddenGrid
    };

    QnMotionGridDisplayActionCondition(Condition condition, QObject *parent = NULL): 
        QnActionCondition(parent),
        m_condition(condition)
    {}

    virtual Qn::ActionVisibility check(const QnResourceWidgetList &widgets) override;

private:
    Condition m_condition;
};


class QnResourceActionCondition: public QnActionCondition {
public:
    enum MatchMode {
        OneMatches, /**< Match if at least one resource satisfies the criterion. */
        AllMatch,   /**< Match only if all resources satisfy the criterion. */
    };

    QnResourceActionCondition(MatchMode matchMode, const QnResourceCriterion &criterion, QObject *parent = NULL);

    virtual ~QnResourceActionCondition();

    virtual Qn::ActionVisibility check(const QnResourceList &resources) override;

    virtual Qn::ActionVisibility check(const QnResourceWidgetList &widgets) override;

protected:
    template<class Item, class ItemSequence>
    bool checkInternal(const ItemSequence &sequence);

    bool checkOne(const QnResourcePtr &resource);

    bool checkOne(QnResourceWidget *widget);

private:
    MatchMode m_matchMode;
    QnResourceCriterion m_criterion;
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


class QnSaveLayoutActionCondition: public QnTargetlessActionCondition {
public:
    QnSaveLayoutActionCondition(bool isCurrent, QObject *parent = NULL): QnTargetlessActionCondition(parent), m_current(isCurrent) {}

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



#endif // QN_ACTION_CONDITIONS_H
