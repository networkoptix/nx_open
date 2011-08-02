#include "resource_param.h"

QnParam::QnParam(const QString& name, QnValue val)
{
    type = Value;
    this->name = name;
    this->value = val;
}


QnParam::QnParam()
{
	type = None;
	readonly = false;
	ui = false;
}

bool QnParam::setValue(QnValue val) // safe way to set value
{
	switch(type)
	{
	case MinMaxStep:
		if (val>max_val) return false;
		if (val<min_val) return false;
		break;

	case None:
		return true;

	case Enumeration:
		if (!possible_values.contains(val))
			return false;
		break;

	}

    value = val;

	return true;
}

bool QnParam::setDefVal(QnValue val) // safe way to set value
{
	switch(type)
	{
	case MinMaxStep:
		if (val>max_val) return false;
		if (val<min_val) return false;
		break;

	case None:
		return false;

	case Enumeration:
		if (!possible_values.contains(val))
			return false;
		break;

	}

	default_value = val;
	return true;
}

//===================================================================================

void QnParamList::inheritedFrom(const QnParamList& other)
{
	MAP::const_iterator it = other.m_params.constBegin();
	for (;it!=other.m_params.constEnd(); ++it)
		m_params[it.key()] = it.value();

}

bool QnParamList::exists(const QString& name) const
{
	MAP::const_iterator it = m_params.find(name);
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
	m_params[param.name] = param;
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
		QString group = param.group;

		if (group=="")
			continue;

		if (!result.contains(group))
			result.push_back(group);
	}

	return result;
}

QList<QString> QnParamList::subGroupList(QString group) const
{
	QList<QString> result;

	foreach (const QnParam& param, m_params)
	{
		QString lgroup = param.group;
		if (lgroup==group)
		{
			QString subgroup = param.subgroup;

			//if (subgroup=="")
			//	continue;

			if (!result.contains(subgroup))
				result.push_back(subgroup);
		}
	}

	return result;
}

QnParamList QnParamList::paramList(QString group, QString subgroup) const
{
	QnParamList result;

	foreach (const QnParam& param, m_params)
	{
		QString lgroup = param.group;
		QString lsubgroup = param.subgroup;

		if (lgroup==group && lsubgroup==subgroup)
			result.put(param);
	}

	return result;

}

QnParamList::MAP& QnParamList::list()
{
	return m_params;
}
