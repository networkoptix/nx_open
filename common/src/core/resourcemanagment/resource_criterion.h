#ifndef QN_RESOURCE_CRITERION_H
#define QN_RESOURCE_CRITERION_H

#include <core/resource/resource_fwd.h>
#include <QVariant>
#include <QMetaType>
#include <QList>

class QRegExp;

struct QN_EXPORT QnResourceCriterion {
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
        GROUP,          /**< Criterion group. */
        CUSTOM = 0x10   /**< Use custom matching function that operates on resource. Target is ignored. */
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

	QnResourceCriterion(const QRegExp &regExp, Target target = SEARCH_STRING, Operation acceptOperation = ACCEPT);

    QnResourceCriterion(int flags, Target target = FLAGS, Operation acceptOperation = ACCEPT);

    QnResourceCriterion(const CriterionFunction &function, const QVariant &targetValue, Operation acceptOperation = ACCEPT);

    QnResourceCriterion(const QVariant &targetValue, Type type, Target target, Operation acceptOperation = ACCEPT);

    QnResourceCriterion();

    ~QnResourceCriterion();

    Operation acceptOperation() const {
        return m_acceptOperation;
    }

    void setAcceptOperation(Operation acceptOperation) {
        m_acceptOperation = acceptOperation;
    }

    bool isGroup() const;

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

    Operation match(const QnResourcePtr &resource) const;

    friend bool operator==(const QnResourceCriterion &l, const QnResourceCriterion &r);

    friend bool operator!=(const QnResourceCriterion &l, const QnResourceCriterion &r) {
        return !(l == r);
    }

protected:
    void *targetValueData();

private:
    Operation m_acceptOperation;
    Type m_type;
    Target m_target;
    QVariant m_targetValue;
    CriterionFunction m_customCriterion;
};

typedef QList<QnResourceCriterion> QnResourceCriterionList;


class QN_EXPORT QnResourceCriterionGroup: public QnResourceCriterion {
public:
    QnResourceCriterionGroup(Operation acceptOperation = ACCEPT);

    void addCriterion(const QnResourceCriterion &criterion);

    bool removeCriterion(const QnResourceCriterion &criterion);

    bool replaceCriterion(const QnResourceCriterion &from, const QnResourceCriterion &to);

    QnResourceCriterionList criteria() const;

    void setCriteria(const QnResourceCriterionList &criteria);
};

Q_DECLARE_METATYPE(QnResourceCriterion);
Q_DECLARE_TYPEINFO(QnResourceCriterion, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(QnResourceCriterionList);

#endif // QN_RESOURCE_CRITERION_H
