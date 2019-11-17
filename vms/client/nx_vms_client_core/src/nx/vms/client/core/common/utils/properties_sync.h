#pragma once

#include <optional>

#include <QtCore/QObject>
#include <QtCore/QVector>
#include <QtCore/QMetaProperty>
#include <QtQml/QQmlListProperty>
#include <QtQml/QJSValue>

namespace nx::vms::client::core {

struct PropertiesSyncProperty: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject* target READ target WRITE setTarget NOTIFY targetChanged)
    Q_PROPERTY(QString property READ propertyName WRITE setPropertyName NOTIFY propertyNameChanged)
    Q_PROPERTY(QJSValue mapFrom READ mapFromFunction WRITE setMapFromFunction
        NOTIFY mapFromFunctionChanged)
    Q_PROPERTY(QJSValue mapTo READ mapToFunction WRITE setMapToFunction
        NOTIFY mapToFunctionChanged)

public:
    PropertiesSyncProperty(QObject* parent = nullptr);

    QObject* target() const { return m_target; }
    void setTarget(QObject* target);

    QString propertyName() const { return m_propertyName; }
    void setPropertyName(const QString& name);

    bool isValid() const;

    std::optional<QVariant> value() const;
    std::optional<QVariant> rawValue() const;
    void setValue(const QVariant& value);

    QJSValue mapFromFunction() const { return m_mapFromFunction; }
    void setMapFromFunction(const QJSValue& function);

    QJSValue mapToFunction() const { return m_mapToFunction; }
    void setMapToFunction(const QJSValue& function);

signals:
    void targetChanged();
    void propertyNameChanged();
    void valueChanged();
    void mapFromFunctionChanged();
    void mapToFunctionChanged();

private:
    QJSEngine* jsEngine() const;
    void updateProperty();

private:
    QObject* m_target = nullptr;
    QString m_propertyName;
    QMetaProperty m_property;
    mutable QJSValue m_mapFromFunction;
    QJSValue m_mapToFunction;
};

class PropertiesSync: public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QQmlListProperty<nx::vms::client::core::PropertiesSyncProperty> properties
        READ synchedProperties)
    Q_PROPERTY(bool initNewProperties MEMBER m_initNewProperties)
    Q_CLASSINFO("DefaultProperty", "properties")

public:
    PropertiesSync(QObject* parent = nullptr);

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);

    QQmlListProperty<PropertiesSyncProperty> synchedProperties();

    Q_INVOKABLE QVariant value() const;
    Q_INVOKABLE void setValue(const QVariant& value);
    Q_INVOKABLE void start(const QVariant& initialValue);

    static void registerQmlTypes();

    void append(PropertiesSyncProperty* property);
    int count() const;
    PropertiesSyncProperty* synchedProperty(int index) const;
    void clear();

signals:
    void enabledChanged();

private:
    void connectToProperty(PropertiesSyncProperty* property);
    void atValueChanged();
    void initProperty();
    std::optional<QVariant> value(PropertiesSyncProperty* ignoredProperty) const;

private:
    bool m_enabled = false;
    QVector<PropertiesSyncProperty*> m_properties;
    bool m_initNewProperties = false;
};

} // namespace nx::vms::client::core
