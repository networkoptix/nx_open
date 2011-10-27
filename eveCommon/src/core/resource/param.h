#ifndef cl_divice_param_h_1020
#define cl_divice_param_h_1020

#include <QSharedPointer>
#include "utils/common/associativearray.h"


struct QN_EXPORT QnParamType
{
    enum DataType {None, Value, OnOff, Boolen, MinMaxStep, Enumeration, Button };

    QnParamType();
    //QnParamType(const QString& name, QnValue val);

    QString name;

    DataType type;

    QnValue min_val;
    QnValue max_val;
    QnValue step;

    QList<QnValue> possible_values;
    QList<QnValue> ui_possible_values;

    QnValue default_value;

    QString paramNetHelper;
    QString group;
    QString subgroup;
    QString description;

    bool ui;
    bool readonly;

    bool setDefVal(QnValue val); // safe way to set value
};
typedef QSharedPointer<QnParamType> QnParamTypePtr;

struct QN_EXPORT QnParam
{
    QnParam() {}
    QnParam(QnParamTypePtr paramType) { m_paramType = paramType;}

    bool setValue(QnValue val); // safe way to set value
    const QnValue& value() const { return m_value; }
    const QString& name() const  { return m_paramType->name; }
    const QString& group() const { return m_paramType->group; }
    const QString& subgroup() const { return m_paramType->subgroup; }
    bool  isUiParam() const  { return m_paramType->ui; }
    const QnValue& minValue() const { return m_paramType->min_val; }
    const QnValue& maxValue() const { return m_paramType->max_val; }
    QnParamType::DataType type() const { return m_paramType->type;}
    const QList<QnValue>& uiPossibleValues() const { return m_paramType->ui_possible_values; }
    const QList<QnValue>& possibleValues() const { return m_paramType->possible_values; }
    const QString& description() const { return m_paramType->description; }
    const bool isReadOnly() const { return m_paramType->readonly; }
private:
    QnParamTypePtr m_paramType;
    QnValue m_value;
};

class QN_EXPORT QnParamList
{

public:
    typedef QMap<QString, QnParam> QnParamMap;

    void inheritedFrom(const QnParamList& other);
    bool exists(const QString& name) const;
    QnParam& get(const QString& name);
    const QnParam get(const QString& name) const;
    void put(const QnParam& param);
    bool empty() const;
    QnParamMap& list();

    QList<QString> groupList() const;
    QList<QString> subGroupList(QString group) const;

    QnParamList paramList(QString group, QString subgroup) const;
private:
    QnParamMap m_params;
};

#endif //cl_divice_param_h_1020
