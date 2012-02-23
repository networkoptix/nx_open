#ifndef QN_ACTION_CONDITIONS_H
#define QN_ACTION_CONDITIONS_H

#include <QObject>
#include <core/resource/resource_fwd.h>
#include "action_fwd.h"

class QnResourceCriterion;

class QnActionCondition: public QObject {
public:
    QnActionCondition(QObject *parent = NULL);

    virtual bool check(const QnResourceList &resources) { 
        Q_UNUSED(resources);

        return false; 
    };

    virtual bool check(const QnResourceWidgetList &widgets) { 
        Q_UNUSED(widgets);
    
        return false;
    };

    virtual bool check(const QVariant &items);
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

    virtual bool check(const QnResourceWidgetList &widgets) override;

private:
    Condition m_condition;
};


class QnResourceActionCondition: public QnActionCondition {
public:
    enum MatchMode {
        OneMatches, /**< Match if at least one resource satisfies the criterion. */
        AllMatch,   /**< Match only if all resources satisfy the criterion. */
    };

    QnResourceActionCondition(MatchMode matchMode, QnResourceCriterion *criterion, QObject *parent = NULL);

    virtual ~QnResourceActionCondition();

    virtual bool check(const QnResourceList &resources) override;

    virtual bool check(const QnResourceWidgetList &widgets) override;

protected:
    template<class Item, class ItemSequence>
    bool checkInternal(const ItemSequence &sequence);

    bool checkOne(const QnResourcePtr &resource);

    bool checkOne(QnResourceWidget *widget);

private:
    MatchMode m_matchMode;
    QScopedPointer<QnResourceCriterion> m_criterion;
};


class QnResourceRemovalActionCondition: public QnActionCondition {
public:
    virtual bool check(const QnResourceList &resources) override;
};


#endif // QN_ACTION_CONDITIONS_H
