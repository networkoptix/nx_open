#include "resource_criteria.h"
#include <QRegExp>
#include <utils/common/warnings.h>
#include <core/resource/resource.h>

namespace {
    bool identityCriteriaFunction(const QnResourcePtr &, const QVariant &) {
        return true;
    }

    QVariant resourceValue(const QnResourcePtr &resource, QnResourceCriteria::Target target) {
        if(resource.isNull())
            return QVariant();

        switch(target) {
        case QnResourceCriteria::ID:            return resource->getId().toString();
        case QnResourceCriteria::TYPE_ID:       return resource->getTypeId().toString();
        case QnResourceCriteria::NAME:          return resource->getName();
        case QnResourceCriteria::SEARCH_STRING: return resource->toSearchString();
        case QnResourceCriteria::STATUS:        return resource->getStatus();
        case QnResourceCriteria::FLAGS:         return resource->flags();
        case QnResourceCriteria::URL:           return resource->getUrl();
        default:
            return QVariant();
        }
    }

} // anonymous namespace

QnResourceCriteria::QnResourceCriteria(const QRegExp &regExp, Target target, Operation operation):
    m_operation(operation),
    m_type(REGEXP),
    m_target(target),
    m_targetValue(regExp),
    m_customCriteria(NULL)
{}

QnResourceCriteria::QnResourceCriteria(int flags, Target target, Operation operation):
    m_operation(operation),
    m_type(FLAG_INCLUSION),
    m_target(target),
    m_targetValue(flags),
    m_customCriteria(NULL)
{}

QnResourceCriteria::QnResourceCriteria(const QVariant &targetValue, Type type, Target target, Operation operation):
    m_operation(operation),
    m_type(type),
    m_target(target),
    m_targetValue(targetValue),
    m_customCriteria(NULL)
{}

QnResourceCriteria::QnResourceCriteria():
    m_operation(INCLUDE),
    m_type(IDENTITY),
    m_target(ID),
    m_targetValue(QVariant()),
    m_customCriteria(NULL)
{}

QnResourceCriteria::~QnResourceCriteria() {
    return;
}

void QnResourceCriteria::setType(Type type) {
    if(type < 0 || type > CUSTOM) {
        qnWarning("Invalid resource criteria type '%1'.", static_cast<int>(type));
        return;
    }

    m_type = type;
    if(m_type == CUSTOM) {
        m_customCriteria = &identityCriteriaFunction;
    } else {
        m_customCriteria = NULL;
    }
}

void QnResourceCriteria::setTarget(Target target) {
    if(target < 0 || target >= TARGET_COUNT) {
        qnWarning("Invalid criteria target '%1'.", static_cast<int>(target));
        return;
    }

    m_target = target;
}

void QnResourceCriteria::setTargetValue(const QVariant &targetValue) {
    switch(m_type) {
    case FLAG_INCLUSION: {
        QVariant tmp = targetValue;
        if(!tmp.convert(QVariant::Int))
            qnWarning("Criteria of type FLAG_INCLUSION expects its target value to be convertible to int.");
        m_targetValue = tmp;
        return;
    }
    case REGEXP:
        if(targetValue.type() != QVariant::RegExp)
            qnWarning("Criteria of type REGEXP expects a QRegExp as its target value.");
        /* FALL THROUGH. */
    case EQUALITY:
    case CUSTOM:
    case IDENTITY:
        m_targetValue = targetValue;
        break;
    default:
        break;
    }
}

QnResourceCriteria::CriteriaFunction QnResourceCriteria::customType() const {
    return m_customCriteria;
}

void QnResourceCriteria::setCustomType(const CriteriaFunction &function) {
    if(function == NULL) {
        qnNullWarning(function);
        setCustomType(&identityCriteriaFunction);
        return;
    }

    m_type = CUSTOM;
    m_customCriteria = function;
}

bool QnResourceCriteria::matches(const QnResourcePtr &resource) const {
    bool result = true;

    switch(m_type) {
    case IDENTITY:
        result = true;
        break;
    case EQUALITY:
        result = m_targetValue == resourceValue(resource, m_target);
        if(!result)
            result = m_targetValue.toString() == resourceValue(resource, m_target).toString();
        break;
    case REGEXP: {
        QRegExp regExp = m_targetValue.toRegExp();
        result = regExp.exactMatch(resourceValue(resource, m_target).toString());
        break;
    }
    case FLAG_INCLUSION: {
        int mask = m_targetValue.toInt();
        int value = resourceValue(resource, m_target).toInt();
        result = (value & mask) == mask;
        break;
    }
    case CUSTOM:
        result = m_customCriteria(resource, m_targetValue);
        break;
    }

    if(m_operation == EXCLUDE)
        result = !result;

    return result;
}

