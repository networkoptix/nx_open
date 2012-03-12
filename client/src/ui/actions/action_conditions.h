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
        Any, /**< Match if at least one resource satisfies the criterion. */
        All, /**< Match only if all resources satisfy the criterion. */
    };

} // namespace Qn


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
    QnMotionGridDisplayActionCondition(bool requiredGridDisplayValue, QObject *parent = NULL): 
        QnActionCondition(parent),
        m_requiredGridDisplayValue(requiredGridDisplayValue)
    {}

    virtual Qn::ActionVisibility check(const QnResourceWidgetList &widgets) override;

private:
    bool m_requiredGridDisplayValue;
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
