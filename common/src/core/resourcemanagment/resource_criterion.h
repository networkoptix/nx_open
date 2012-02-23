#ifndef QN_RESOURCE_CRITERION_H
#define QN_RESOURCE_CRITERION_H

#include <core/resource/resource_fwd.h>
#include <QVariant>
#include <QMetaType>
#include <QList>

class QRegExp;

class QnResourceCriterionGroup;

class QN_EXPORT QnResourceCriterion {
public:
    enum Operation {
        ACCEPT,         /**< Accept the resource. */
        REJECT,         /**< Reject the resource. */
        NEXT            /**< Criterion has nothing to say of whether the resource should be accepted or rejected. Next criterion should be invoked. */
    };

	enum Type {
        NOTHING,        /**< Match no resources. Target is ignored. */
        EQUALITY,       /**< One of the resource's fields must equal provided value. */
        CONTAINMENT,    /**< One of the resource's fields must contain provided value. */
        REGEXP,         /**< One of the resource's fields must match the provided regular expression. */
        GROUP,          /**< Sequential criterion group. Target is ignored. */
        CUSTOM          /**< Use custom matching function that operates on resource. Target is ignored. */
    };

    enum Target {
        ID,             /**< Match resource's id. */
        TYPE_ID,        /**< Match resource's type id. */ 
        UNIQUE_ID,      /**< Match resource's unique id. */
        NAME,           /**< Match resource's name. */
        SEARCH_STRING,  /**< Match resource's search string. */
        STATUS,         /**< Match resource's status. */
        FLAGS,          /**< Match resource's flags. */
        URL,            /**< Match resource's url. */
        TAGS,           /**< Match resource's tags. */
        TARGET_COUNT
    };

    typedef Operation (*CriterionFunction)(const QnResourcePtr &, const QVariant &value);

	QnResourceCriterion(const QRegExp &regExp, Target target = SEARCH_STRING, Operation matchOperation = ACCEPT, Operation mismatchOperation = NEXT);

    QnResourceCriterion(int flags, Target target = FLAGS, Operation matchOperation = ACCEPT, Operation mismatchOperation = NEXT);

    QnResourceCriterion(const CriterionFunction &function, const QVariant &targetValue, Operation matchOperation = ACCEPT, Operation mismatchOperation = NEXT);

    QnResourceCriterion(const QVariant &targetValue, Type type, Target target, Operation matchOperation = ACCEPT, Operation mismatchOperation = NEXT);

    QnResourceCriterion();

    virtual ~QnResourceCriterion();

    Operation matchOperation() const {
        return m_matchOperation;
    }

    void setMatchOperation(Operation matchOperation) {
        m_matchOperation = matchOperation;
    }
    
    Operation mismatchOperation() const {
        return m_mismatchOperation;
    }

    void setMismatchOperation(Operation mismatchOperation) {
        m_mismatchOperation = mismatchOperation;
    }

    Operation nextOperation() const {
        return m_nextOperation;
    }

    void setNextOperation(Operation nextOperation) {
        m_nextOperation = nextOperation;
    }

    bool isGroup() const;

    QnResourceCriterionGroup *asGroup();

    Type type() const {
        return m_type;
    }

    void setType(Type type);

    Target target() const {
        return m_target;
    }

    void setTarget(Target target);

    const QVariant &targetValue() const {
        return m_targetValue;
    }

    void setTargetValue(const QVariant &targetValue);

    CriterionFunction customType() const;

    void setCustomType(const CriterionFunction &function);

    Operation check(const QnResourcePtr &resource) const;

    QnResourceList filter(const QnResourceList &resources);

protected:
    void *targetValueData();

private:
    Q_DISABLE_COPY(QnResourceCriterion);

    union {
        struct {
            Operation m_matchOperation : 4;
            Operation m_mismatchOperation : 4;
            Operation m_nextOperation : 8;
            Type m_type : 8;
            Target m_target : 8;
        };

        int m_padding;
    };
    QVariant m_targetValue;
    CriterionFunction m_customCriterion;
};

typedef QList<QnResourceCriterion *> QnResourceCriterionList;


class QN_EXPORT QnResourceCriterionGroup: public QnResourceCriterion {
public:
    QnResourceCriterionGroup(const QString &pattern, Operation matchOperation = ACCEPT, Operation mismatchOperation = NEXT);

    QnResourceCriterionGroup(Operation matchOperation = ACCEPT, Operation mismatchOperation = NEXT);

    void addCriterion(QnResourceCriterion *criterion);

    bool removeCriterion(QnResourceCriterion *criterion);

    bool replaceCriterion(QnResourceCriterion *from, QnResourceCriterion *to);

    void clear();

    QnResourceCriterionList criteria() const;

    void setCriteria(const QnResourceCriterionList &criteria);

    void setPattern(const QString &pattern);

private:
    Q_DISABLE_COPY(QnResourceCriterionGroup);
};

Q_DECLARE_METATYPE(QnResourceCriterion *);
Q_DECLARE_METATYPE(QnResourceCriterionList);


namespace QnResourceCriterionExpressions {

    class QnResourceCriterionExpression {
    public:
        QnResourceCriterionExpression(QnResourceCriterion *criterion): m_criterion(criterion) {}

        operator QnResourceCriterion *() {
            return m_criterion;
        }

    private:
        QnResourceCriterion *m_criterion;
    };

    inline QnResourceCriterionExpression hasFlags(int flags) {
        return new QnResourceCriterion(flags);
    }

    inline QnResourceCriterionExpression hasStatus(int status) {
        return new QnResourceCriterion(status, QnResourceCriterion::EQUALITY, QnResourceCriterion::STATUS);
    }

    inline QnResourceCriterionExpression operator||(QnResourceCriterionExpression &le, QnResourceCriterionExpression &re) {
        QnResourceCriterion *l = le, *r = re;

        QnResourceCriterionGroup *result;
        if(l->isGroup() && l->asGroup()->nextOperation() == QnResourceCriterion::REJECT) { /* Is OR operation. */
            result = l->asGroup();

            r->setMatchOperation(QnResourceCriterion::ACCEPT);
            r->setMismatchOperation(QnResourceCriterion::NEXT);

            result->addCriterion(r);
        } else {
            result = new QnResourceCriterionGroup();
            l->setMatchOperation(QnResourceCriterion::ACCEPT);
            l->setMismatchOperation(QnResourceCriterion::NEXT);

            r->setMatchOperation(QnResourceCriterion::ACCEPT);
            r->setMismatchOperation(QnResourceCriterion::NEXT);

            result->setNextOperation(QnResourceCriterion::REJECT);
        }

        return result;
    }

    inline QnResourceCriterionExpression operator&&(QnResourceCriterionExpression &le, QnResourceCriterionExpression &re) {
        QnResourceCriterion *l = le, *r = re;

        QnResourceCriterionGroup *result;
        if(l->isGroup() && l->asGroup()->nextOperation() == QnResourceCriterion::ACCEPT) { /* Is AND operation. */
            result = l->asGroup();

            r->setMatchOperation(QnResourceCriterion::NEXT);
            r->setMismatchOperation(QnResourceCriterion::REJECT);

            result->addCriterion(r);
        } else {
            result = new QnResourceCriterionGroup();
            l->setMatchOperation(QnResourceCriterion::NEXT);
            l->setMismatchOperation(QnResourceCriterion::REJECT);

            r->setMatchOperation(QnResourceCriterion::NEXT);
            r->setMismatchOperation(QnResourceCriterion::REJECT);

            result->setNextOperation(QnResourceCriterion::ACCEPT);
        }

        return result;
    }

} // namespace QnResourceCriterionExpressions


#endif // QN_RESOURCE_CRITERION_H
