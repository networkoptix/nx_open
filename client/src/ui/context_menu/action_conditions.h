#ifndef QN_ACTION_CONDITIONS_H
#define QN_ACTION_CONDITIONS_H

#include <QObject>
#include <core/resource/resource_fwd.h>

class QGraphicsItem;

class QnResourceCriterion;

class QnActionCondition: public QObject {
public:
    QnActionCondition(QObject *parent = NULL): QObject(parent) {}

    virtual bool check(const QnResourceList &resources) { 
        Q_UNUSED(resources);

        return false; 
    };

    virtual bool check(const QList<QGraphicsItem *> &items) { 
        Q_UNUSED(items);
    
        return false;
    };

protected:
    QnResourcePtr resource(QGraphicsItem *item) const;
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

    virtual bool check(const QList<QGraphicsItem *> &items) override;

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

    virtual bool check(const QList<QGraphicsItem *> &items) override;

protected:
    template<class Item, class ItemSequence>
    bool checkInternal(const ItemSequence &sequence);

    bool checkOne(const QnResourcePtr &resource);

    bool checkOne(QGraphicsItem *item);

private:
    MatchMode m_matchMode;
    QScopedPointer<QnResourceCriterion> m_criterion;
};


#endif // QN_ACTION_CONDITIONS_H
