#include "resource_criterion.h"
#include <QtCore/QRegExp>
#include <utils/common/warnings.h>
#include <core/resource/resource.h>

namespace {
    QnResourceCriterion::Operation nextCriterionFunction(const QnResourcePtr &, const QVariant &) {
        return QnResourceCriterion::Next;
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
QnResourceCriterion::QnResourceCriterion(const QRegExp &regExp, const char *propertyName, Operation matchOperation, Operation mismatchOperation):
    m_matchOperation(matchOperation),
    m_mismatchOperation(mismatchOperation),
    m_nextOperation(Next),
    m_type(RegExp),
    m_propertyName(propertyName),
    m_targetValue(regExp),
    m_customCriterion(NULL)
{}

QnResourceCriterion::QnResourceCriterion(Qn::ResourceFlags flags, const char *propertyName, Operation matchOperation, Operation mismatchOperation):
    m_matchOperation(matchOperation),
    m_mismatchOperation(mismatchOperation),
    m_nextOperation(Next),
    m_type(Containment),
    m_propertyName(propertyName),
    m_targetValue(flags),
    m_customCriterion(NULL)
{}

QnResourceCriterion::QnResourceCriterion(Qn::CameraCapabilities cameraCapabilities, const char *propertyName, Operation matchOperation, Operation mismatchOperation):
    m_matchOperation(matchOperation),
    m_mismatchOperation(mismatchOperation),
    m_nextOperation(Next),
    m_type(Containment),
    m_propertyName(propertyName),
    m_targetValue(cameraCapabilities),
    m_customCriterion(NULL)
{}

QnResourceCriterion::QnResourceCriterion(Qn::PtzCapabilities ptzCapabilities, const char *propertyName, Operation matchOperation, Operation mismatchOperation):
    m_matchOperation(matchOperation),
    m_mismatchOperation(mismatchOperation),
    m_nextOperation(Next),
    m_type(Containment),
    m_propertyName(propertyName),
    m_targetValue(ptzCapabilities),
    m_customCriterion(NULL)
{}

QnResourceCriterion::QnResourceCriterion(const CriterionFunction &function, const QVariant &targetValue, Operation matchOperation, Operation mismatchOperation):
    m_matchOperation(matchOperation),
    m_mismatchOperation(mismatchOperation),
    m_nextOperation(Next),
    m_type(Nothing),
    m_propertyName(NULL),
    m_targetValue(targetValue),
    m_customCriterion(NULL)
{
    setCustomType(function);
}

QnResourceCriterion::QnResourceCriterion(const QVariant &targetValue, Type type, const char *propertyName, Operation matchOperation, Operation mismatchOperation):
    m_matchOperation(matchOperation),
    m_mismatchOperation(mismatchOperation),
    m_nextOperation(Next),
    m_type(Nothing),
    m_propertyName(propertyName),
    m_targetValue(targetValue),
    m_customCriterion(NULL)
{
    setType(type);
}

QnResourceCriterion::QnResourceCriterion():
    m_matchOperation(Accept),
    m_mismatchOperation(Next),
    m_nextOperation(Next),
    m_type(Nothing),
    m_propertyName(NULL),
    m_targetValue(QVariant()),
    m_customCriterion(NULL)
{}

QnResourceCriterion::~QnResourceCriterion() {
    return;
}

bool QnResourceCriterion::isGroup() const {
    return m_type == Group;
}

QnResourceCriterionGroup &QnResourceCriterion::asGroup() {
    assert(isGroup());

    return static_cast<QnResourceCriterionGroup &>(*this);
}

const QnResourceCriterionGroup &QnResourceCriterion::asGroup() const {
    assert(isGroup());

    return static_cast<const QnResourceCriterionGroup &>(*this);
}

void QnResourceCriterion::setType(Type type) {
    if(m_type == type)
        return;

    if(type < 0 || type > Custom) {
        qnWarning("Invalid resource criterion type '%1'.", static_cast<int>(type));
        return;
    }

    m_type = type;

    if(m_type == Custom) {
        m_customCriterion = &nextCriterionFunction;
    } else {
        m_customCriterion = NULL;
    }

    if(isGroup() && m_targetValue.userType() != qn_criterionListMetaTypeId)
        m_targetValue = QVariant::fromValue<QnResourceCriterionList>(QnResourceCriterionList());

    if(m_type == RegExp && m_targetValue.userType() != QVariant::RegExp)
        m_targetValue = QVariant::fromValue<QRegExp>(QRegExp());
}

void QnResourceCriterion::setPropertyName(const char *propertyName) {
    m_propertyName = propertyName;
}

void QnResourceCriterion::setTargetValue(const QVariant &targetValue) {
    switch(m_type) {
    case QnResourceCriterionGroup::Group:
        if(targetValue.userType() != qn_criterionListMetaTypeId) {
            qnWarning("Criterion group expects its target value to be an instance of QnResourceCriterionList.");
            return;
        }
        m_targetValue = targetValue;
        break;
    case RegExp:
        if(targetValue.type() != QVariant::RegExp) {
            qnWarning("Criterion of type RegExp expects a QRegExp as its target value.");
            return;
        }
        /* FALL THROUGH. */
    case Containment:
    case Equality:
    case Custom:
    case Nothing:
        m_targetValue = targetValue;
        break;
    default:
        break;
    }
}

void *QnResourceCriterion::targetValueData() {
    return m_targetValue.data();
}

const void *QnResourceCriterion::targetValueData() const {
    return m_targetValue.constData();
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

    m_type = Custom;
    m_customCriterion = function;
}

QnResourceCriterion::Operation QnResourceCriterion::check(const QnResourcePtr &resource) const {
    if(!resource) {
        qnNullWarning(resource);
        return m_mismatchOperation;
    }

    Operation result = Reject;

    switch(m_type) {
    case Nothing:
        break;
    case Equality: {
        QVariant value = resource->property(m_propertyName);
        if(m_targetValue == value) {
            result = Accept;
        } else if(m_targetValue.toString() == value.toString()) {
            result = Accept;
        }
        break;
    }
    case RegExp: {
        QRegExp regExp = m_targetValue.toRegExp();
        if(regExp.exactMatch(resource->property(m_propertyName).toString()))
            result = Accept;
        break;
    }
    case Containment: {
        QVariant value = resource->property(m_propertyName);
        if(value.userType() == QVariant::StringList) {
            if(value.toStringList().contains(m_targetValue.toString(), Qt::CaseInsensitive))
                result = Accept;
        } else if(value.userType() == QVariant::Int) {
            int mask = m_targetValue.toInt();
            if((value.toInt() & mask) == mask)
                result = Accept;
        } else {
            if(value.toString().contains(m_targetValue.toString(), Qt::CaseInsensitive))
                result = Accept;
        }
        break;
    }
    case Group:
        foreach(const QnResourceCriterion &criterion, static_cast<const QnResourceCriterionGroup *>(this)->criteria()) {
            result = criterion.check(resource);
            if(result != Next)
                break;
        }
        break;
    case Custom:
        result = m_customCriterion(resource, m_targetValue);
        break;
    }

    switch(result) {
    case Accept: return m_matchOperation;
    case Reject: return m_mismatchOperation;
    default: return m_nextOperation;
    }
}

bool operator==(const QnResourceCriterion &l, const QnResourceCriterion &r) {
    if(l.type() != r.type())
        return false;

    if(l.matchOperation() != r.matchOperation() || l.mismatchOperation() != r.mismatchOperation() || l.nextOperation() != r.nextOperation())
        return false;

    switch(l.type()) {
    case QnResourceCriterion::Nothing:
        return true;
    case QnResourceCriterion::Equality:
    case QnResourceCriterion::Containment:
    case QnResourceCriterion::RegExp: {
        bool equalPropertyNames = false;
        if(l.propertyName() == r.propertyName()) {
            equalPropertyNames = true;
        } else if(l.propertyName() == NULL || r.propertyName() == NULL) {
            equalPropertyNames = false;
        } else {
            equalPropertyNames = strcmp(l.propertyName(), r.propertyName()) == 0;
        }
        if(!equalPropertyNames)
            return false;

        return l.targetValue() == r.targetValue();
    }
    case QnResourceCriterion::Group: {
        return l.asGroup().criteria() == r.asGroup().criteria();
    }
    case QnResourceCriterion::Custom:
        return l.customType() == r.customType() && l.targetValue() == r.targetValue();
    default:
        qnWarning("Unreachable code executed.");
        return false;
    }
}


// -------------------------------------------------------------------------- //
// QnResourceCriterionGroup
// -------------------------------------------------------------------------- //
QnResourceCriterionGroup::QnResourceCriterionGroup(const QString &pattern, Operation matchOperation, Operation mismatchOperation) {
    qn_initCriterionListMetaType();

    setType(Group);
    setMatchOperation(matchOperation);
    setMismatchOperation(mismatchOperation);
    setPattern(pattern);
}

QnResourceCriterionGroup::QnResourceCriterionGroup(Operation matchOperation, Operation mismatchOperation) {
    qn_initCriterionListMetaType();

    setType(Group);
    setMatchOperation(matchOperation);
    setMismatchOperation(mismatchOperation);
}

void QnResourceCriterionGroup::addCriterion(const QnResourceCriterion &criterion) {
    assert(targetValue().userType() == qn_criterionListMetaTypeId);

    QnResourceCriterionList *d = static_cast<QnResourceCriterionList *>(targetValueData());
    d->push_front(criterion);
}

bool QnResourceCriterionGroup::removeCriterion(const QnResourceCriterion &criterion) {
    assert(targetValue().userType() == qn_criterionListMetaTypeId);

    QnResourceCriterionList *d = static_cast<QnResourceCriterionList *>(targetValueData());
    return d->removeOne(criterion);
}

void QnResourceCriterionGroup::clear() {
    setCriteria(QnResourceCriterionList());
}

const QnResourceCriterionList &QnResourceCriterionGroup::criteria() const {
    assert(targetValue().userType() == qn_criterionListMetaTypeId);

    return *static_cast<const QnResourceCriterionList *>(targetValueData());
}

bool QnResourceCriterionGroup::empty() const {
    return criteria().empty();
}

int QnResourceCriterionGroup::size() const {
    return criteria().size();
}

void QnResourceCriterionGroup::setCriteria(const QnResourceCriterionList &criteria) {
    assert(targetValue().userType() == qn_criterionListMetaTypeId);

    setTargetValue(QVariant::fromValue<QnResourceCriterionList>(criteria));
}

void QnResourceCriterionGroup::setPattern(const QString &pattern) {
    clear();

    /* Empty pattern matches everything. */
    if (pattern.isEmpty()) {
        addCriterion(QnResourceCriterion(QVariant(), Nothing, NULL, Accept, Accept));
        return;
    }

    /* Normalize pattern. */
    QString normalizedPattern = pattern;
    normalizedPattern.replace(QLatin1Char('"'), QString());

    /* It may have become empty as a result of normalization. */
    if(normalizedPattern.isEmpty())
        return;

    /* Reject by default. */
    addCriterion(QnResourceCriterion(QVariant(), Nothing, NULL, Reject, Reject));

    /* Parse pattern string. */
    QRegExp spaces(QLatin1String("[\\s]+"), Qt::CaseSensitive, QRegExp::RegExp2);
    int pos = 0;
    bool positive = true;
    for(int i = 0, size = normalizedPattern.size(); i <= size; i++) {
        QChar c;
        if(i == size) {
            c = QLatin1Char('+'); /* So that chunk parsing logic is triggered. */
        } else {
            c = normalizedPattern[i];
        }
        if(c != QLatin1Char('\\') && c != QLatin1Char('+'))
            continue;

        QnResourceCriterionGroup group = QnResourceCriterionGroup(Next, Next);
        group.setNextOperation(positive ? Accept : Reject);

        QString chunks = normalizedPattern.mid(pos, i - pos);
        pos = i + 1;
        foreach(const QString &chunk, chunks.split(spaces, QString::SkipEmptyParts)) {
            QString key;
            QString pattern;
            
            int index = chunk.indexOf(QLatin1Char(':'));
            if(index != -1) {
                key = chunk.left(index);
                pattern = chunk.mid(index + 1);
            } else {
                pattern = chunk;
            }

            if (pattern.isEmpty())
                continue;

            Type type = Containment;
            const char *propertyName = QnResourceProperty::searchString;
            QVariant targetValue = pattern;
            if (key == QLatin1String("id")) {
                propertyName = QnResourceProperty::id;
                type = Equality;
            } else if (key == QLatin1String("name")) {
                propertyName = QnResourceProperty::name;
            } else if (key == QLatin1String("tag")) {
                propertyName = QnResourceProperty::tags;
            } else if (key == QLatin1String("type")) {
                propertyName = QnResourceProperty::flags;

                if(pattern == QLatin1String("camera")) {
                    targetValue = static_cast<int>(Qn::live);
                } else if(pattern == QLatin1String("image")) {
                    targetValue = static_cast<int>(Qn::still_image);
                } else if(pattern == QLatin1String("video")) {
                    targetValue = static_cast<int>(Qn::local | Qn::video);
                } else {
                    targetValue = 0xFFFFFFFF;
                }
            } else if (pattern == QLatin1String("live")) {
                propertyName = QnResourceProperty::flags;
                targetValue = static_cast<int>(Qn::live);
            }

            group.addCriterion(QnResourceCriterion(targetValue, type, propertyName, Next, Reject));
        }

        if(!group.criteria().empty())
            addCriterion(group);

        positive = c == QLatin1Char('+');
    }
}



