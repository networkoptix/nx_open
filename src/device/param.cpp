#include "param.h"

CLParamType::CLParamType()
{
	type = None;
	synchronized = false;
	readonly = false;
	ui = false;
}

bool CLParamType::setValue(CLValue val, bool set) // safe way to set value
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

	if (set)
		value = val;

	return true;
}

bool CLParamType::setDefVal(CLValue val) // safe way to set value
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

void CLParamList::inheritedFrom(const CLParamList& other)
{
	MAP::const_iterator it = other.m_params.constBegin();
	for (;it!=other.m_params.constEnd(); ++it)
		m_params[it.key()] = it.value();

}

bool CLParamList::exists(const QString& name) const
{
	MAP::const_iterator it = m_params.find(name);
	if (it == m_params.end())
		return false;

	return true;
}

CLParam& CLParamList::get(const QString& name)
{
	return m_params[name];
}

const CLParam CLParamList::get(const QString& name) const
{
	return m_params[name];
}

void CLParamList::put(const CLParam& param)
{
	m_params[param.name] = param;
}

bool CLParamList::empty() const
{
	return m_params.empty();
}

QList<QString> CLParamList::groupList() const
{
	QList<QString> result;

	foreach (const CLParam &param, m_params)
	{
		const QString &group = param.value.group;
		if (!group.isEmpty() && !result.contains(group))
			result.push_back(group);
	}

	return result;
}

QList<QString> CLParamList::subGroupList(const QString &group) const
{
	QList<QString> result;

	foreach (const CLParam &param, m_params)
	{
		const QString &lgroup = param.value.group;
		if (lgroup == group)
		{
			const QString &subgroup = param.value.subgroup;
			if (/*!subgroup.isEmpty() && */!result.contains(subgroup))
				result.push_back(subgroup);
		}
	}

	return result;
}

CLParamList CLParamList::paramList(const QString &group, const QString &subgroup) const
{
	CLParamList result;

	foreach (const CLParam &param, m_params)
	{
		if (param.value.group == group && param.value.subgroup == subgroup)
			result.put(param);
	}

	return result;

}

CLParamList::MAP &CLParamList::list()
{
	return m_params;
}
