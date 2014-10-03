
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

//===================================================================================

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
