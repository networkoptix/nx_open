#ifndef QN_PARAM_H
#define QN_PARAM_H

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QSharedPointer>
#include <QtCore/QVariant>

struct QN_EXPORT QnParamType
{
    enum DataType { None, Value, OnOff, Boolen, MinMaxStep, Enumeration, Button };

    QnParamType();
    //QnParamType(const QString& name, const QVariant &val);

    DataType type;

    QString name;
    QVariant default_value;

    double min_val;
    double max_val;
    double step;

    QList<QVariant> possible_values;
    QList<QVariant> ui_possible_values;

    QString paramNetHelper;
    QString group;
    QString subgroup;
    QString description;

    bool ui;
    bool isReadOnly;
    bool isStatic;

    bool setDefVal(const QVariant &val); // safe way to set value
};

typedef QSharedPointer<QnParamType> QnParamTypePtr;


struct QN_EXPORT QnParam
{
    QnParam() {}
    QnParam(const QVariant &val) { m_value = val; }
    QnParam(QnParamTypePtr paramType) { m_paramType = paramType;}

    bool setValue(const QVariant &val); // safe way to set value
    const QVariant& value() const { return m_value; }
    const QString& name() const { return m_paramType->name; }
    const QString& group() const { return m_paramType->group; }
    const QString& subgroup() const { return m_paramType->subgroup; }
    double minValue() const { return m_paramType->min_val; }
    double maxValue() const { return m_paramType->max_val; }
    QnParamType::DataType type() const { return m_paramType->type;}
    const QList<QVariant>& uiPossibleValues() const { return m_paramType->ui_possible_values; }
    const QList<QVariant>& possibleValues() const { return m_paramType->possible_values; }
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
    QVariant m_value;
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
