#ifndef QN_RESOURCE_CRITERION_H
#define QN_RESOURCE_CRITERION_H

#include <QtCore/QVariant>
#include <QtCore/QMetaType>
#include <QtCore/QList>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/resource_property.h>

class QRegExp;

class QnResourceCriterionGroup;

// TODO: #Elric this one works more like a filter. Rename.
class QN_EXPORT QnResourceCriterion {
public:
    enum Operation {
        Accept,         /**< Accept the resource. */
        Reject,         /**< Reject the resource. */
        Next            /**< Criterion has nothing to say of whether the resource should be accepted or rejected. Next criterion should be invoked. */
    };

    enum Type {
        Nothing,        /**< Match no resources. Target is ignored. */
        Equality,       /**< One of the resource's fields must equal provided value. */
        Containment,    /**< One of the resource's fields must contain provided value. */
        RegExp,         /**< One of the resource's fields must match the provided regular expression. */
        Group,          /**< Sequential criterion group. Target is ignored. */
        Custom          /**< Use custom matching function that operates on resource. Target is ignored. */
    };

    typedef Operation (*CriterionFunction)(const QnResourcePtr &, const QVariant &value);

    explicit QnResourceCriterion(const QRegExp &regExp, const char *propertyName = QnResourceProperty::searchString, Operation matchOperation = Accept, Operation mismatchOperation = Next);

    explicit QnResourceCriterion(Qn::ResourceFlags flags, const char *propertyName = QnResourceProperty::flags, Operation matchOperation = Accept, Operation mismatchOperation = Next);

    explicit QnResourceCriterion(Qn::CameraCapabilities cameraCapabilities, const char *propertyName = QnResourceProperty::cameraCapabilities, Operation matchOperation = Accept, Operation mismatchOperation = Next);

    explicit QnResourceCriterion(Qn::PtzCapabilities ptzCapabilities, const char *propertyName = QnResourceProperty::ptzCapabilities, Operation matchOperation = Accept, Operation mismatchOperation = Next);

    explicit QnResourceCriterion(const CriterionFunction &function, const QVariant &targetValue, Operation matchOperation = Accept, Operation mismatchOperation = Next);

    explicit QnResourceCriterion(const QVariant &targetValue, Type type, const char *propertyName, Operation matchOperation = Accept, Operation mismatchOperation = Next);

    explicit QnResourceCriterion();

    ~QnResourceCriterion();

    bool isNull() const {
        return m_type == Nothing;
    }

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

    QnResourceCriterionGroup &asGroup();

    const QnResourceCriterionGroup &asGroup() const;

    Type type() const {
        return m_type;
    }

    void setType(Type type);

    const char *propertyName() const {
        return m_propertyName;
    }

    void setPropertyName(const char *propertyName);

    const QVariant &targetValue() const {
        return m_targetValue;
    }

    void setTargetValue(const QVariant &targetValue);

    CriterionFunction customType() const;

    void setCustomType(const CriterionFunction &function);

    Operation check(const QnResourcePtr &resource) const;

    template<class Resource>
    QnSharedResourcePointerList<Resource> filter(const QnSharedResourcePointerList<Resource> &resources) const {
        QnSharedResourcePointerList<Resource> result;
        for(const QnSharedResourcePointer<Resource> &resource: resources) {
            Operation operation = check(resource);
            if(operation == Accept)
                result.push_back(resource);
        }
        return result;
    }

    friend bool operator==(const QnResourceCriterion &l, const QnResourceCriterion &r);

protected:
    void *targetValueData();

    const void *targetValueData() const;

private:
    union {
        struct {
            Operation m_matchOperation : 4;
            Operation m_mismatchOperation : 4;
            Operation m_nextOperation : 4;
            Type m_type : 4;
        };

        int m_padding;
    };
    const char *m_propertyName;
    QVariant m_targetValue;
    CriterionFunction m_customCriterion;
};

typedef QList<QnResourceCriterion> QnResourceCriterionList;


class QN_EXPORT QnResourceCriterionGroup: public QnResourceCriterion {
public:
    QnResourceCriterionGroup(const QString &pattern, Operation matchOperation = Accept, Operation mismatchOperation = Next);

    QnResourceCriterionGroup(Operation matchOperation = Accept, Operation mismatchOperation = Next);

    void addCriterion(const QnResourceCriterion &criterion);

    bool removeCriterion(const QnResourceCriterion &criterion);

    void clear();

    bool empty() const;
    
    int size() const;

    const QnResourceCriterionList &criteria() const;

    void setCriteria(const QnResourceCriterionList &criteria);

    void setPattern(const QString &pattern);
};

Q_DECLARE_METATYPE(QnResourceCriterion);
Q_DECLARE_METATYPE(QnResourceCriterionList);


template<class Resource>
QnSharedResourcePointerList<Resource> QnSharedResourcePointerList<Resource>::filtered(const QnResourceCriterion &criterion) const {
    return criterion.filter(*this);
}


namespace QnResourceCriterionExpressions {

    class QnResourceCriterionExpression {
    public:
        QnResourceCriterionExpression(QnResourceCriterion criterion): m_criterion(criterion) {}

        operator QnResourceCriterion () const {
            return m_criterion;
        }

    private:
        QnResourceCriterion m_criterion;
    };

    inline QnResourceCriterionExpression hasFlags(Qn::ResourceFlags flags) {
        return QnResourceCriterion(flags);
    }

    inline QnResourceCriterionExpression hasStatus(Qn::ResourceStatus status) {
        return QnResourceCriterion(status, QnResourceCriterion::Equality, QnResourceProperty::status);
    }

    inline QnResourceCriterionExpression hasCapabilities(Qn::CameraCapabilities capabilities) {
        return QnResourceCriterion(capabilities);
    }

    inline QnResourceCriterionExpression operator||(const QnResourceCriterionExpression &le, const QnResourceCriterionExpression &re) {
        QnResourceCriterion l = le, r = re;

        QnResourceCriterionGroup result;
        if(l.isGroup() && l.asGroup().nextOperation() == QnResourceCriterion::Reject) { /* Is OR operation. */
            result = l.asGroup();

            r.setMatchOperation(QnResourceCriterion::Accept);
            r.setMismatchOperation(QnResourceCriterion::Next);

            result.addCriterion(r);
        } else {
            result = QnResourceCriterionGroup();
            l.setMatchOperation(QnResourceCriterion::Accept);
            l.setMismatchOperation(QnResourceCriterion::Next);
            result.addCriterion(l);

            r.setMatchOperation(QnResourceCriterion::Accept);
            r.setMismatchOperation(QnResourceCriterion::Next);
            result.addCriterion(r);

            result.setNextOperation(QnResourceCriterion::Reject);
        }

        return result;
    }

    inline QnResourceCriterionExpression operator&&(const QnResourceCriterionExpression &le, const QnResourceCriterionExpression &re) {
        QnResourceCriterion l = le, r = re;

        QnResourceCriterionGroup result;
        if(l.isGroup() && l.asGroup().nextOperation() == QnResourceCriterion::Accept) { /* Is AND operation. */
            result = l.asGroup();

            r.setMatchOperation(QnResourceCriterion::Next);
            r.setMismatchOperation(QnResourceCriterion::Reject);

            result.addCriterion(r);
        } else {
            result = QnResourceCriterionGroup();
            l.setMatchOperation(QnResourceCriterion::Next);
            l.setMismatchOperation(QnResourceCriterion::Reject);
            result.addCriterion(l);

            r.setMatchOperation(QnResourceCriterion::Next);
            r.setMismatchOperation(QnResourceCriterion::Reject);
            result.addCriterion(r);

            result.setNextOperation(QnResourceCriterion::Accept);
        }

        return result;
    }

} // namespace QnResourceCriterionExpressions


#endif // QN_RESOURCE_CRITERION_H
