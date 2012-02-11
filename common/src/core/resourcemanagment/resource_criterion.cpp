#include "resource_criterion.h"
#include <QRegExp>
#include <utils/common/warnings.h>
#include <core/resource/resource.h>

namespace {
    QnResourceCriterion::Operation nextCriterionFunction(const QnResourcePtr &, const QVariant &) {
        return QnResourceCriterion::NEXT;
    }

    QVariant resourceValue(const QnResourcePtr &resource, QnResourceCriterion::Target target) {
        if(resource.isNull())
            return QVariant();

        switch(target) {
        case QnResourceCriterion::ID:               return resource->getId().toString();
        case QnResourceCriterion::TYPE_ID:          return resource->getTypeId().toString();
        case QnResourceCriterion::NAME:             return resource->getName();
        case QnResourceCriterion::SEARCH_STRING:    return resource->toSearchString();
        case QnResourceCriterion::STATUS:           return static_cast<int>(resource->getStatus());
        case QnResourceCriterion::FLAGS:            return static_cast<int>(resource->flags());
        case QnResourceCriterion::URL:              return resource->getUrl();
        case QnResourceCriterion::TAGS:             return resource->tagList();
        default:
            return QVariant();
        }
    }

    static int qn_criterionListMetaTypeId = 0;

} // anonymous namespace

// -------------------------------------------------------------------------- //
// QnResourceCriterion
// -------------------------------------------------------------------------- //
QnResourceCriterion::QnResourceCriterion(const QRegExp &regExp, Target target, Operation acceptOperation):
    m_acceptOperation(acceptOperation),
    m_type(REGEXP),
    m_target(target),
    m_targetValue(regExp),
    m_customCriterion(NULL)
{}

QnResourceCriterion::QnResourceCriterion(int flags, Target target, Operation acceptOperation):
    m_acceptOperation(acceptOperation),
    m_type(CONTAINMENT),
    m_target(target),
    m_targetValue(flags),
    m_customCriterion(NULL)
{}

QnResourceCriterion::QnResourceCriterion(const CriterionFunction &function, const QVariant &targetValue, Operation acceptOperation):
    m_acceptOperation(acceptOperation),
    m_type(NOTHING),
    m_target(ID),
    m_targetValue(targetValue),
    m_customCriterion(NULL)
{
    setCustomType(function);
}

QnResourceCriterion::QnResourceCriterion(const QVariant &targetValue, Type type, Target target, Operation acceptOperation):
    m_acceptOperation(acceptOperation),
    m_type(type),
    m_target(target),
    m_targetValue(targetValue),
    m_customCriterion(NULL)
{}

QnResourceCriterion::QnResourceCriterion():
    m_acceptOperation(ACCEPT),
    m_type(NOTHING),
    m_target(ID),
    m_targetValue(QVariant()),
    m_customCriterion(NULL)
{}

QnResourceCriterion::~QnResourceCriterion() {
    return;
}

bool QnResourceCriterion::isGroup() const {
    return m_type == GROUP;
}

void QnResourceCriterion::setType(Type type) {
    if(m_type == type)
        return;

    if(type < 0 || type > CUSTOM) {
        qnWarning("Invalid resource criterion type '%1'.", static_cast<int>(type));
        return;
    }

    m_type = type;
    if(m_type == CUSTOM) {
        m_customCriterion = &nextCriterionFunction;
    } else {
        m_customCriterion = NULL;
    }

    if(isGroup() && m_targetValue.userType() != qn_criterionListMetaTypeId)
        m_targetValue = QVariant::fromValue<QnResourceCriterionList>(QnResourceCriterionList());
}

void QnResourceCriterion::setTarget(Target target) {
    if(target < 0 || target >= TARGET_COUNT) {
        qnWarning("Invalid criterion target '%1'.", static_cast<int>(target));
        return;
    }

    m_target = target;
}

void QnResourceCriterion::setTargetValue(const QVariant &targetValue) {
    switch(m_type) {
    case QnResourceCriterionGroup::GROUP:
        if(targetValue.userType() != qn_criterionListMetaTypeId) {
            qnWarning("Criterion group expects its target value to be an instance of QnResourceCriterionList.");
            return;
        }
        m_targetValue = targetValue;
        break;
    case REGEXP:
        if(targetValue.type() != QVariant::RegExp) {
            qnWarning("Criterion of type REGEXP expects a QRegExp as its target value.");
            return;
        }
    case CONTAINMENT:
    case EQUALITY:
    case CUSTOM:
    case NOTHING:
        m_targetValue = targetValue;
        break;
    default:
        break;
    }
}

void *QnResourceCriterion::targetValueData() {
    return m_targetValue.data();
}

QnResourceCriterion::CriterionFunction QnResourceCriterion::customType() const {
    return m_customCriterion;
}

void QnResourceCriterion::setCustomType(const CriterionFunction &function) {
    if(function == NULL) {
        qnNullWarning(function);
        setCustomType(&nextCriterionFunction);
        return;
    }

    m_type = CUSTOM;
    m_customCriterion = function;
}

QnResourceCriterion::Operation QnResourceCriterion::match(const QnResourcePtr &resource) const {
    Operation result = NEXT;

    switch(m_type) {
    case NOTHING:
        break;
    case EQUALITY:
        if(m_targetValue == resourceValue(resource, m_target)) {
            result = ACCEPT;
        } else if(m_targetValue.toString() == resourceValue(resource, m_target).toString()) {
            result = ACCEPT;
        }
        break;
    case REGEXP: {
        QRegExp regExp = m_targetValue.toRegExp();
        if(regExp.exactMatch(resourceValue(resource, m_target).toString()))
            result = ACCEPT;
        break;
    }
    case CONTAINMENT: {
        QVariant value = resourceValue(resource, m_target);
        if(value.userType() == QVariant::StringList) {
            if(value.toStringList().contains(m_targetValue.toString()))
                result = ACCEPT;
        } else if(value.userType() == QVariant::Int) {
            int mask = m_targetValue.toInt();
            if((value.toInt() & mask) == mask)
                result = ACCEPT;
        } else {
            if(value.toString().contains(m_targetValue.toString()))
                result = ACCEPT;
        }
        break;
    }
    case QnResourceCriterionGroup::GROUP:
        foreach(const QnResourceCriterion &criterion, static_cast<const QnResourceCriterionGroup *>(this)->criteria()) {
            result = criterion.match(resource);
            if(result != NEXT)
                break;
        }
        break;
    case CUSTOM:
        result = m_customCriterion(resource, m_targetValue);
        break;
    }

    if(m_acceptOperation == ACCEPT) {
        return result;
    } else {
        switch(result) {
        case ACCEPT: return REJECT;
        case REJECT: return ACCEPT;
        default: return NEXT;
        }
    }
}

bool operator==(const QnResourceCriterion &l, const QnResourceCriterion &r) {
    if(l.m_acceptOperation != r.m_acceptOperation)
        return false;

    if(l.m_type == r.m_type)
        return false;

    if(l.m_type == QnResourceCriterion::NOTHING)
        return true;

    if(l.isGroup()) {
        if(static_cast<const QnResourceCriterionGroup &>(l).criteria() != static_cast<const QnResourceCriterionGroup &>(r).criteria())
            return false;
    } else {
        if(l.m_targetValue != r.m_targetValue)
            return false;
    }

    if(l.m_type == QnResourceCriterion::CUSTOM && l.m_customCriterion != r.m_customCriterion)
        return false;

    return true;
}


// -------------------------------------------------------------------------- //
// QnResourceCriterionGroup
// -------------------------------------------------------------------------- //
QnResourceCriterionGroup::QnResourceCriterionGroup(Operation acceptOperation) {
    setType(GROUP);
    setAcceptOperation(acceptOperation);

    if(qn_criterionListMetaTypeId == 0)
        qn_criterionListMetaTypeId = qMetaTypeId<QnResourceCriterionList>();
}

void QnResourceCriterionGroup::addCriterion(const QnResourceCriterion &criterion) {
    assert(targetValue().type() == qn_criterionListMetaTypeId);

    QnResourceCriterionList *d = static_cast<QnResourceCriterionList *>(targetValueData());
    d->push_front(criterion);
}

bool QnResourceCriterionGroup::removeCriterion(const QnResourceCriterion &criterion) {
    assert(targetValue().type() == qn_criterionListMetaTypeId);

    QnResourceCriterionList *d = static_cast<QnResourceCriterionList *>(targetValueData());
    return d->removeOne(criterion);
}

bool QnResourceCriterionGroup::replaceCriterion(const QnResourceCriterion &from, const QnResourceCriterion &to) {
    assert(targetValue().type() == qn_criterionListMetaTypeId);

    QnResourceCriterionList *d = static_cast<QnResourceCriterionList *>(targetValueData());
    int index = d->indexOf(from);
    if(index < 0)
        return false;

    d->replace(index, to);
    return true;
}

QnResourceCriterionList QnResourceCriterionGroup::criteria() const {
    assert(targetValue().type() == qn_criterionListMetaTypeId);

    return targetValue().value<QnResourceCriterionList>();
}

void QnResourceCriterionGroup::setCriteria(const QnResourceCriterionList &criteria) {
    assert(targetValue().type() == qn_criterionListMetaTypeId);

    setTargetValue(QVariant::fromValue<QnResourceCriterionList>(criteria));
}

