#ifndef QN_PARAM_H
#define QN_PARAM_H

#include <QtCore/QSharedPointer>
#include <QtCore/QVariant>

typedef QVariant QnValue;

struct QN_EXPORT QnParamType
{
    enum DataType { None, Value, OnOff, Boolen, MinMaxStep, Enumeration, Button };

    QnParamType();
    //QnParamType(const QString& name, QnValue val);

    QString name;

    DataType type;

    double min_val;
    double max_val;
    double step;

    QList<QnValue> possible_values;
    QList<QnValue> ui_possible_values;

    QnValue default_value;

    QString paramNetHelper;
    QString group;
    QString subgroup;
    QString description;

    bool ui;
    bool isReadOnly;
    bool isStatic;

    bool setDefVal(const QnValue &val); // safe way to set value
};

typedef QSharedPointer<QnParamType> QnParamTypePtr;


struct QN_EXPORT QnParam
{
    QnParam() {}
    QnParam(const QnValue &val) { m_value = val; }
    QnParam(QnParamTypePtr paramType) { m_paramType = paramType;}

    bool setValue(const QnValue &val); // safe way to set value
    const QnValue& value() const { return m_value; }
    const QString& name() const { return m_paramType->name; }
    const QString& group() const { return m_paramType->group; }
    const QString& subgroup() const { return m_paramType->subgroup; }
    double minValue() const { return m_paramType->min_val; }
    double maxValue() const { return m_paramType->max_val; }
    QnParamType::DataType type() const { return m_paramType->type;}
    const QList<QnValue>& uiPossibleValues() const { return m_paramType->ui_possible_values; }
    const QList<QnValue>& possibleValues() const { return m_paramType->possible_values; }
    const QString& description() const { return m_paramType->description; }
    bool isUiParam() const { return m_paramType->ui; }
    bool isStatic() const { return m_paramType->isStatic; }
    const bool isReadOnly() const { return m_paramType->isReadOnly; }
    const QString netHelper() const { return m_paramType->paramNetHelper; }

    QString toDebugString() const
    {
        return name() + QLatin1Char(' ') + m_paramType->default_value.toString();
    }

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
    QList<QString> subGroupList(const QString &group) const;

    QnParamList paramList(const QString &group, const QString &subgroup) const;

private:
    QnParamMap m_params;
};

#endif // QN_PARAM_H
