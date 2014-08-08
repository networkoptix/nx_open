
#include "param.h"


/*
QnParamType::QnParamType(const QString& name, const QVariant &val)
    : type(Value), name(name), default_value(val),
      min_val(0.0), max_val(0.0), step(0.0),
      ui(false), isReadOnly(false), isPhysical(true)
{
    type = Value;
    this->name = name;
    this->value = val;
}
*/

QnParamType::QnParamType()
    : type(Qn::PDT_None),
      min_val(0.0), max_val(0.0), step(0.0),
      ui(false), isReadOnly(false), isPhysical(true)
{
}

bool QnParamType::setDefVal(const QVariant &val) // safe way to set value
{
    switch (type)
    {
    case Qn::PDT_None:
        return false;

    case Qn::PDT_MinMaxStep:
        if (val.toDouble() < min_val || val.toDouble() > max_val)
            return false;
        break;

    case Qn::PDT_Enumeration:
        if (!possible_values.contains(val))
            return false;
        break;

    default:
        break;
    }

    default_value = val;
    return true;
}

//===================================================================================

void QnParamList::unite(const QnParamList &other)
{
    QnParamMap::const_iterator it = other.m_params.constBegin();
    for ( ; it != other.m_params.constEnd(); ++it)
        m_params[it.key()] = it.value();
}

bool QnParamList::contains(const QString &name) const
{
    return m_params.contains(name);
}

QnParam &QnParamList::operator[](const QString &name)
{
    return m_params[name];
}

const QnParam QnParamList::operator[](const QString &name) const
{
    return m_params[name];
}

const QnParam QnParamList::value(const QString &name) const
{
    return m_params.value(name);
}

void QnParamList::append(const QnParam &param)
{
    m_params.insert(param.name(), param);
}

bool QnParamList::isEmpty() const
{
    return m_params.isEmpty();
}

QList<QnParam> QnParamList::list() const
{
    return m_params.values();
}

QList<QString> QnParamList::keys() const
{
    return m_params.keys();
}

QList<QString> QnParamList::groupList() const
{
    QList<QString> result;
    result.append(QString(0, QChar()));

    foreach (const QnParam &param, m_params) {
        const QString &group = param.group();
        if (!group.isEmpty() && !result.contains(group))
            result.append(group);
    }

    return result;
}

QList<QString> QnParamList::subGroupList(const QString &group) const
{
    QList<QString> result;
    result.append(QString(0, QChar()));

    foreach (const QnParam &param, m_params) {
        const QString &lgroup = param.group();
        if (lgroup == group) {
            const QString &subGroup = param.subGroup();
            if (!subGroup.isEmpty() && !result.contains(subGroup))
                result.append(subGroup);
        }
    }

    return result;
}

QnParamList QnParamList::paramList(const QString &group, const QString &subGroup) const
{
    QnParamList result;

    foreach (const QnParam &param, m_params) {
        const QString &lgroup = param.group();
        const QString &lsubGroup = param.subGroup();
        if (lgroup == group && lsubGroup == subGroup)
            result.append(param);
    }

    return result;
}

// ===========================================
// QnParam
// ===========================================

bool QnParam::setValue(const QVariant &val) // safe way to set value
{
    switch(m_paramType->type)
    {
    case Qn::PDT_None:
        return true;

    case Qn::PDT_MinMaxStep:
        if (val.toDouble() < m_paramType->min_val || val.toDouble() > m_paramType->max_val)
            return false;
        break;

    case Qn::PDT_Enumeration:
        if (!m_paramType->possible_values.contains(val))
            return false;
        break;

    default:
        break;
    }

    m_value = val;

    return true;
}
