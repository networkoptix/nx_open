#include "param.h"

/*
QnParamType::QnParamType(const QString& name, QnValue val)
{
    type = Value;
    this->name = name;
    this->value = val;
}
*/

QnParamType::QnParamType()
    : type(None),
      min_val(0.0), max_val(0.0), step(0.0),
      ui(false), isReadOnly(false), isStatic(false)
{
}

bool QnParamType::setDefVal(const QnValue &val) // safe way to set value
{
    switch (type)
    {
    case None:
        return false;

    case MinMaxStep:
        if (val.toDouble() < min_val || val.toDouble() > max_val)
            return false;
        break;

    case Enumeration:
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

void QnParamList::inheritedFrom(const QnParamList& other)
{
    QnParamMap::const_iterator it = other.m_params.constBegin();
    for (;it!=other.m_params.constEnd(); ++it)
        m_params[it.key()] = it.value();

}

bool QnParamList::exists(const QString& name) const
{
    QnParamMap::const_iterator it = m_params.find(name);
    if (it == m_params.end())
        return false;

    return true;
}

QnParam& QnParamList::get(const QString& name)
{
    return m_params[name];
}

const QnParam QnParamList::get(const QString& name) const
{
    return m_params[name];
}

void QnParamList::put(const QnParam& param)
{
    m_params[param.name()] = param;
}

bool QnParamList::empty() const
{
    return m_params.empty();
}

QList<QString> QnParamList::groupList() const
{
    QList<QString> result;

    foreach (const QnParam& param, m_params)
    {
        QString group = param.group();

        if (group=="")
            continue;

        if (!result.contains(group))
            result.push_back(group);
    }

    return result;
}

QList<QString> QnParamList::subGroupList(const QString &group) const
{
    QList<QString> result;

    foreach (const QnParam& param, m_params)
    {
        QString lgroup = param.group();
        if (lgroup==group)
        {
            QString subgroup = param.subgroup();

            //if (subgroup=="")
            //	continue;

            if (!result.contains(subgroup))
                result.push_back(subgroup);
        }
    }

    return result;
}

QnParamList QnParamList::paramList(const QString &group, const QString &subgroup) const
{
    QnParamList result;

    foreach (const QnParam& param, m_params)
    {
        QString lgroup = param.group();
        QString lsubgroup = param.subgroup();

        if (lgroup == group && lsubgroup == subgroup)
            result.put(param);
    }

    return result;

}

QnParamList::QnParamMap& QnParamList::list()
{
    return m_params;
}

// ===========================================
// QnParam
// ===========================================

bool QnParam::setValue(const QnValue &val) // safe way to set value
{
    switch(m_paramType->type)
    {
    case QnParamType::None:
        return true;

    case QnParamType::MinMaxStep:
        if (val.toDouble() < m_paramType->min_val || val.toDouble() > m_paramType->max_val)
            return false;
        break;

    case QnParamType::Enumeration:
        if (!m_paramType->possible_values.contains(val))
            return false;
        break;

    default:
        break;
    }

    m_value = val;

    return true;
}
