#ifndef cl_divice_param_h_1020
#define cl_divice_param_h_1020

#include "common/associativearray.h"


struct CLParamType
{
	enum {None, Value, OnOff, Boolen, MinMaxStep, Enumeration, Button };	

	CLParamType();

	int type;

	QnValue min_val;
	QnValue max_val;
	QnValue step;

	QList<QnValue> possible_values;
	QList<QnValue> ui_possible_values;

	QnValue value;
	QnValue default_value;

	QString http;
	QString group;
	QString subgroup;
	QString description;
	bool ui;

	bool readonly;

	bool synchronized;

	bool setValue(QnValue val, bool set = true); // safe way to set value
	bool setDefVal(QnValue val); // safe way to set value

};

struct CLParam
{
	QString name;
	CLParamType value;
};

class QnParamList
{

public:
	typedef QMap<QString, CLParam> MAP;

	void inheritedFrom(const QnParamList& other);
	bool exists(const QString& name) const;
	CLParam& get(const QString& name);
	const CLParam get(const QString& name) const;
	void put(const CLParam& param);
	bool empty() const;
	MAP& list();

	QList<QString> groupList() const;
	QList<QString> subGroupList(QString group) const;

	QnParamList paramList(QString group, QString subgroup) const;
private:
	MAP m_params;
};

#endif //cl_divice_param_h_1020
