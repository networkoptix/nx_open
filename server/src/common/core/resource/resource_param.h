#ifndef cl_divice_param_h_1020
#define cl_divice_param_h_1020

#include "common/associativearray.h"


struct QnParam
{
	enum {None, Value, OnOff, Boolen, MinMaxStep, Enumeration, Button };	

	QnParam();
    QnParam(const QString& name, QnValue val);

    QString name;

	int type;

	QnValue min_val;
	QnValue max_val;
	QnValue step;

	QList<QnValue> possible_values;
	QList<QnValue> ui_possible_values;

	QnValue value;
	QnValue default_value;

	QString paramNetHelper;
	QString group;
	QString subgroup;
	QString description;

	bool ui;
	bool readonly;


	bool setValue(QnValue val); // safe way to set value
	bool setDefVal(QnValue val); // safe way to set value

};


class QnParamList
{

public:
	typedef QMap<QString, QnParam> MAP;

	void inheritedFrom(const QnParamList& other);
	bool exists(const QString& name) const;
	QnParam& get(const QString& name);
	const QnParam get(const QString& name) const;
	void put(const QnParam& param);
	bool empty() const;
	MAP& list();

	QList<QString> groupList() const;
	QList<QString> subGroupList(QString group) const;

	QnParamList paramList(QString group, QString subgroup) const;
private:
	MAP m_params;
};

#endif //cl_divice_param_h_1020
