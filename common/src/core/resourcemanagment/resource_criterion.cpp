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

    /**
     * This function is currently unused, but it may come in handy later. Don't delete. 
     */
    QRegExp buildFilter(const QList<QString> &parts) {
        QString pattern;
        foreach (const QString &part, parts) {
            if (!pattern.isEmpty())
                pattern += QLatin1Char('|');
            pattern += QRegExp::escape(part);
        }
        pattern = QLatin1String(".*(") + pattern + QLatin1String(").*");
        return QRegExp(pattern, Qt::CaseInsensitive, QRegExp::RegExp2);
    }

    static int qn_criterionListMetaTypeId = 0;

    void qn_initCriterionListMetaType() {
        if(qn_criterionListMetaTypeId == 0)
            qn_criterionListMetaTypeId = qMetaTypeId<QnResourceCriterionList>();
    }

} // anonymous namespace

// -------------------------------------------------------------------------- //
// QnResourceCriterion
// -------------------------------------------------------------------------- //
QnResourceCriterion::QnResourceCriterion(const QRegExp &regExp, Target target, Operation matchOperation, Operation mismatchOperation):
    m_matchOperation(matchOperation),
    m_mismatchOperation(mismatchOperation),
    m_type(REGEXP),
    m_target(target),
    m_targetValue(regExp),
    m_customCriterion(NULL)
{}

QnResourceCriterion::QnResourceCriterion(int flags, Target target, Operation matchOperation, Operation mismatchOperation):
    m_matchOperation(matchOperation),
    m_mismatchOperation(mismatchOperation),
    m_type(CONTAINMENT),
    m_target(target),
    m_targetValue(flags),
    m_customCriterion(NULL)
{}

QnResourceCriterion::QnResourceCriterion(const CriterionFunction &function, const QVariant &targetValue, Operation matchOperation, Operation mismatchOperation):
    m_matchOperation(matchOperation),
    m_mismatchOperation(mismatchOperation),
    m_type(NOTHING),
    m_target(ID),
    m_targetValue(targetValue),
    m_customCriterion(NULL)
{
    setCustomType(function);
}

QnResourceCriterion::QnResourceCriterion(const QVariant &targetValue, Type type, Target target, Operation matchOperation, Operation mismatchOperation):
    m_matchOperation(matchOperation),
    m_mismatchOperation(mismatchOperation),
    m_type(NOTHING),
    m_target(target),
    m_targetValue(targetValue),
    m_customCriterion(NULL)
{
    setType(type);
}

QnResourceCriterion::QnResourceCriterion():
    m_matchOperation(ACCEPT),
    m_mismatchOperation(NEXT),
    m_type(NOTHING),
    m_target(ID),
    m_targetValue(QVariant()),
    m_customCriterion(NULL)
{}

QnResourceCriterion::~QnResourceCriterion() {
    if(isGroup())
        qDeleteAll(static_cast<const QnResourceCriterionGroup *>(this)->criteria());
}

bool QnResourceCriterion::isGroup() const {
    return m_type == GROUP;
}

QnResourceCriterionGroup *QnResourceCriterion::asGroup() {
    return isGroup() ? static_cast<QnResourceCriterionGroup *>(this) : NULL;
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

    if(m_type == REGEXP && m_targetValue.userType() != QVariant::RegExp)
        m_targetValue = QVariant::fromValue<QRegExp>(QRegExp());
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
        /* FALL THROUGH. */
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

QnResourceCriterion::Operation QnResourceCriterion::check(const QnResourcePtr &resource) const {
    Operation result = REJECT;

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
            if(value.toStringList().contains(m_targetValue.toString(), Qt::CaseInsensitive))
                result = ACCEPT;
        } else if(value.userType() == QVariant::Int) {
            int mask = m_targetValue.toInt();
            if((value.toInt() & mask) == mask)
                result = ACCEPT;
        } else {
            if(value.toString().contains(m_targetValue.toString(), Qt::CaseInsensitive))
                result = ACCEPT;
        }
        break;
    }
    case GROUP:
        foreach(const QnResourceCriterion *criterion, static_cast<const QnResourceCriterionGroup *>(this)->criteria()) {
            result = criterion->check(resource);
            if(result != NEXT)
                break;
        }
        break;
    case CUSTOM:
        result = m_customCriterion(resource, m_targetValue);
        break;
    }

    switch(result) {
    case ACCEPT: return m_matchOperation;
    case REJECT: return m_mismatchOperation;
    default: return m_nextOperation;
    }
}

QnResourceList QnResourceCriterion::filter(const QnResourceList &resources) {
    QnResourceList result;
    foreach(const QnResourcePtr &resource, resources) {
        Operation operation = check(resource);
        if(operation == ACCEPT)
            result.push_back(resource);
    }
    return result;
}


// -------------------------------------------------------------------------- //
// QnResourceCriterionGroup
// -------------------------------------------------------------------------- //
QnResourceCriterionGroup::QnResourceCriterionGroup(const QString &pattern, Operation matchOperation, Operation mismatchOperation) {
    qn_initCriterionListMetaType();

    setType(GROUP);
    setMatchOperation(matchOperation);
    setMismatchOperation(mismatchOperation);
    setPattern(pattern);
}

QnResourceCriterionGroup::QnResourceCriterionGroup(Operation matchOperation, Operation mismatchOperation) {
    qn_initCriterionListMetaType();

    setType(GROUP);
    setMatchOperation(matchOperation);
    setMismatchOperation(mismatchOperation);
}

void QnResourceCriterionGroup::addCriterion(QnResourceCriterion *criterion) {
    assert(targetValue().userType() == qn_criterionListMetaTypeId);

    QnResourceCriterionList *d = static_cast<QnResourceCriterionList *>(targetValueData());
    d->push_front(criterion);
}

bool QnResourceCriterionGroup::removeCriterion(QnResourceCriterion *criterion) {
    assert(targetValue().userType() == qn_criterionListMetaTypeId);

    QnResourceCriterionList *d = static_cast<QnResourceCriterionList *>(targetValueData());
    return d->removeOne(criterion);
}

bool QnResourceCriterionGroup::replaceCriterion(QnResourceCriterion *from, QnResourceCriterion *to) {
    assert(targetValue().userType() == qn_criterionListMetaTypeId);

    QnResourceCriterionList *d = static_cast<QnResourceCriterionList *>(targetValueData());
    int index = d->indexOf(from);
    if(index < 0)
        return false;

    d->replace(index, to);
    return true;
}

void QnResourceCriterionGroup::clear() {
    setCriteria(QnResourceCriterionList());
}

QnResourceCriterionList QnResourceCriterionGroup::criteria() const {
    assert(targetValue().userType() == qn_criterionListMetaTypeId);

    return targetValue().value<QnResourceCriterionList>();
}

void QnResourceCriterionGroup::setCriteria(const QnResourceCriterionList &criteria) {
    assert(targetValue().userType() == qn_criterionListMetaTypeId);

    setTargetValue(QVariant::fromValue<QnResourceCriterionList>(criteria));
}

void QnResourceCriterionGroup::setPattern(const QString &pattern) {
    clear();

    /* Empty pattern matches nothing. */
    if (pattern.isEmpty())
        return;

    /* Normalize pattern. */
    QString normalizedPattern = pattern;
    normalizedPattern.replace(QChar('"'), QString());

    /* It may have become empty as a result of normalization. */
    if(normalizedPattern.isEmpty())
        return;

    /* Reject by default. */
    addCriterion(new QnResourceCriterion(QVariant(), NOTHING, ID, REJECT, REJECT));

    /* Parse pattern string. */
    QRegExp regExp(QLatin1String("(\\w+:)?(\\w*)|([\\-\\+])"), Qt::CaseSensitive, QRegExp::RegExp2);
    int pos = 0;

    bool positive = true;
    while ((pos = regExp.indexIn(normalizedPattern, pos)) != -1) {
        pos += regExp.matchedLength();
        if (regExp.matchedLength() == 0)
            break;

        const QString sign = regExp.cap(3);
        if(!sign.isEmpty()) {
            positive = sign == QLatin1String("+");
            continue;
        }

        const QString key = regExp.cap(1).toLower();
        const QString pattern = regExp.cap(2);
        if (pattern.isEmpty())
            continue;

        Type type = CONTAINMENT;
        Target target = SEARCH_STRING;
        QVariant targetValue = pattern;
        if (key == QLatin1String("id:")) {
            target = ID;
            type = EQUALITY;
        } else if (key == QLatin1String("name:")) {
            target = NAME;
        } else if (key == QLatin1String("tag:")) {
            target = TAGS;
        } else if (key == QLatin1String("type:")) {
            target = FLAGS;

            if(pattern == "camera") {
                targetValue = static_cast<int>(QnResource::live);
            } else if(pattern == "image") {
                targetValue = static_cast<int>(QnResource::still_image);
            } else if(pattern == "video") {
                targetValue = static_cast<int>(QnResource::local | QnResource::video);
            } else {
                targetValue = 0xFFFFFFFF;
            }
        } else if (pattern == QLatin1String("live")) {
            target = FLAGS;
            targetValue = static_cast<int>(QnResource::live);
        }

        addCriterion(new QnResourceCriterion(targetValue, type, target, positive ? ACCEPT : REJECT, NEXT));
    }
}



