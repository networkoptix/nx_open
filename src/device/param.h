#ifndef cl_divice_param_h_1020
#define cl_divice_param_h_1020

#include "../base/associativearray.h"
#include <QList>
#include <QString>


struct CLParamType
{
	enum {None, Value, Boolen, MinMaxStep, Enumeration };	

	CLParamType()
	{
		type = None;
		synchronized = false;
		readonly = false;
	}

	int type;

	CLValue min_val;
	CLValue max_val;
	CLValue step;

	QList<CLValue> possible_values;

	CLValue value;
	CLValue default_value;

	QString http;
	QString group;
	QString subgroup;
	QString description;

	bool readonly;

	bool synchronized;



	bool setValue(CLValue val, bool set = true) // safe way to set value
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

	bool setDefVal(CLValue val) // safe way to set value
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


};




struct CLParam
{
	QString name;
	CLParamType value;
	//QString description;
	//QString group;
	//QString subgroup;
};


class CLParamList
{
	typedef QMap<QString, CLParam> MAP;
	

	MAP m_params;
public:
	void inheritedFrom(const CLParamList& other)
	{
		MAP::const_iterator it = other.m_params.constBegin();
		for (;it!=other.m_params.constEnd(); ++it)
			m_params[it.key()] = it.value();

	}

	bool exists(const QString& name) const
	{
		MAP::const_iterator it = m_params.find(name);
		if (it == m_params.end())
			return false;

		return true;
	}

	CLParam& get(const QString& name) 
	{
		return m_params[name];
	}

	const CLParam& get(const QString& name) const 
	{
		return m_params[name];
	}

	void put(const CLParam& param) 
	{
		m_params[param.name] = param;
	}

	bool empty() const
	{
		return m_params.empty();
	}

};


#endif //cl_divice_param_h_1020