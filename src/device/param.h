#ifndef cl_divice_param_h_1020
#define cl_divice_param_h_1020

#include "../base/associativearray.h"

struct CLParamType
{
	enum {None, Value, OnOff, Boolen, MinMaxStep, Enumeration, Button };	

	CLParamType();

	int type;

	CLValue min_val;
	CLValue max_val;
	CLValue step;

	QList<CLValue> possible_values;
	QList<CLValue> ui_possible_values;

	CLValue value;
	CLValue default_value;

	QString http;
	QString group;
	QString subgroup;
	QString description;
	bool ui;

	bool readonly;

	bool synchronized;

	bool setValue(CLValue val, bool set = true); // safe way to set value
	bool setDefVal(CLValue val); // safe way to set value

};

struct CLParam
{
	QString name;
	CLParamType value;
};

class CLParamList
{

public:
	typedef QMap<QString, CLParam> MAP;

	void inheritedFrom(const CLParamList& other);
	bool exists(const QString& name) const;
	CLParam& get(const QString& name);
	const CLParam get(const QString& name) const;
	void put(const CLParam& param);
	bool empty() const;
	MAP& list();

	QList<QString> groupList() const;
	QList<QString> subGroupList(QString group) const;

	CLParamList paramList(QString group, QString subgroup) const;
private:
	MAP m_params;
};

#endif //cl_divice_param_h_1020