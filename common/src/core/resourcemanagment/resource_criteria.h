#ifndef QN_RESOURCE_CRITERIA_H
#define QN_RESOURCE_CRITERIA_H

#include <core/resource/resource_fwd.h>
#include <QVariant>

class QRegExp;

struct QN_EXPORT QnResourceCriteria {
public:
    typedef bool (*CriteriaFunction)(const QnResourcePtr &, const QVariant &value);

    enum Operation {
        INCLUDE,        /**< Resources should match the criteria's condition. */
        EXCLUDE         /**< Resources should not match the criteria's condition. */
    };

	enum Type {
        IDENTITY,       /**< Match all resources. Target is ignored. */
        EQUALITY,       /**< One of the resource's fields must equal some value. */
        REGEXP,         /**< Match resources using a regular expression on one of the fields. */
        FLAG_INCLUSION, /**< One of resource's flag fields must include given flags. */
        CUSTOM          /**< Use custom matching function that operates on resource. In this case target is ignored. */
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
        TARGET_COUNT
    };

	QnResourceCriteria(const QRegExp &regExp, Target target = SEARCH_STRING, Operation operation = INCLUDE);

    QnResourceCriteria(int flags, Target target = FLAGS, Operation operation = INCLUDE);

    QnResourceCriteria(const QVariant &targetValue, Type type, Target target, Operation operation = INCLUDE);

    QnResourceCriteria();

    ~QnResourceCriteria();

    Operation operation() const {
        return m_operation;
    }

    void setOperation(Operation operation) {
        m_operation = operation;
    }

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

    CriteriaFunction customType() const;

    void setCustomType(const CriteriaFunction &function);

    bool matches(const QnResourcePtr &resource) const;

protected:
    Operation m_operation;
    Type m_type;
    Target m_target;
    QVariant m_targetValue;
    CriteriaFunction m_customCriteria;
};

#endif // QN_RESOURCE_CRITERIA_H
