#ifndef QN_PARAM_H
#define QN_PARAM_H

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QSharedPointer>
#include <QtCore/QVariant>

#include <utils/common/id.h>

#include "resource_fwd.h"

// TODO: #Elric #enum
enum QnDomain
{
    QnDomainMemory = 1,
    QnDomainDatabase = 2,
    QnDomainPhysical = 4
};

struct QN_EXPORT QnParamType
{
    // TODO: #Elric #enum
    enum DataType { None, Value, OnOff, Boolen, MinMaxStep, Enumeration, Button };

    QnParamType();
    //QnParamType(const QString& name, const QVariant &val);

    DataType type;

    //QnId id;
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
    bool isPhysical;

    bool setDefVal(const QVariant &val); // safe way to set value
};


struct QN_EXPORT QnParam
{
    QnParam(): m_paramType(new QnParamType), m_domain(QnDomainMemory) {}

    QnParam(QnParamTypePtr paramType, const QVariant &value = QVariant(), QnDomain domain = QnDomainMemory): m_paramType(paramType), m_domain(domain) { 
        setValue(value); 
    }

    bool isValid() const { return !m_paramType->name.isEmpty(); }

    bool setValue(const QVariant &val); // safe way to set value
    const QVariant &value() const { return m_value; }
    QnDomain domain() const { return m_domain; }
    void setDomain(QnDomain domain) { m_domain = domain; }
    const QVariant &defaultValue() const { return m_paramType->default_value; }
    const QString &name() const { return m_paramType->name; }
    const QString &group() const { return m_paramType->group; }
    const QString &subGroup() const { return m_paramType->subgroup; }
    double minValue() const { return m_paramType->min_val; }
    double maxValue() const { return m_paramType->max_val; }
    QnParamType::DataType type() const { return m_paramType->type;}
    const QList<QVariant> &uiPossibleValues() const { return m_paramType->ui_possible_values; }
    const QList<QVariant> &possibleValues() const { return m_paramType->possible_values; }
    const QString &description() const { return m_paramType->description; }
    bool isUiParam() const { return m_paramType->ui; }
    bool isPhysical() const { return m_paramType->isPhysical; }
    bool isReadOnly() const { return m_paramType->isReadOnly; }
    const QString &netHelper() const { return m_paramType->paramNetHelper; }
    //const QnId &paramTypeId() const { return m_paramType->id; }

private:
    QnParamTypePtr m_paramType;
    QVariant m_value;
    QnDomain m_domain;
};

Q_DECLARE_METATYPE(QnParam)

class QN_EXPORT QnParamList
{
public:
    void unite(const QnParamList &other);
    bool contains(const QString &name) const;
    QnParam &operator[](const QString &name);
    const QnParam operator[](const QString &name) const;
    const QnParam value(const QString &name) const;
    void append(const QnParam &param);
    bool isEmpty() const;
    QList<QnParam> list() const;
    QList<QString> keys() const;

    QList<QString> groupList() const;
    QList<QString> subGroupList(const QString &group) const;

    QnParamList paramList(const QString &group, const QString &subGroup = QString()) const;

private:
    typedef QHash<QString, QnParam> QnParamMap;
    QnParamMap m_params;
};

#endif // QN_PARAM_H
