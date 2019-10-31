#include "properties_sync.h"

#include <QtQml/QtQml>

namespace nx::vms::client::core {

namespace {

void appendProperty(
    QQmlListProperty<PropertiesSyncProperty>* list, PropertiesSyncProperty* property)
{
    static_cast<PropertiesSync*>(list->object)->append(property);
}

int propertiesCount(QQmlListProperty<PropertiesSyncProperty>* list)
{
    return static_cast<PropertiesSync*>(list->object)->count();
}

PropertiesSyncProperty* getProperty(QQmlListProperty<PropertiesSyncProperty>* list, int index)
{
    return static_cast<PropertiesSync*>(list->object)->synchedProperty(index);
}

void clearProperties(QQmlListProperty<PropertiesSyncProperty>* list)
{
    static_cast<PropertiesSync*>(list->object)->clear();
}

} // namespace

PropertiesSyncProperty::PropertiesSyncProperty(QObject* parent):
    QObject(parent)
{
}

void PropertiesSyncProperty::setTarget(QObject* target)
{
    if (m_target == target)
        return;

    m_target = target;
    updateProperty();
    emit targetChanged();
}

void PropertiesSyncProperty::setPropertyName(const QString& name)
{
    if (m_propertyName == name)
        return;

    m_propertyName = name;
    updateProperty();
    emit propertyNameChanged();
}

bool PropertiesSyncProperty::isValid() const
{
    return m_target && m_property.isValid();
}

std::optional<QVariant> PropertiesSyncProperty::value() const
{
    const std::optional<QVariant>& value = rawValue();
    if (!value)
        return {};

    if (m_mapFromFunction.isCallable())
    {
        if (const auto engine = jsEngine())
        {
            return engine->fromScriptValue<QVariant>(
                m_mapFromFunction.call(QJSValueList{engine->toScriptValue(*value)}));
        }
    }

    return *value;
}

std::optional<QVariant> PropertiesSyncProperty::rawValue() const
{
    if (!isValid())
        return {};

    return m_property.read(m_target);
}

void PropertiesSyncProperty::setValue(const QVariant& value)
{
    if (!isValid())
        return;

    QVariant val = value;

    if (m_mapToFunction.isCallable())
    {
        if (const auto engine = jsEngine())
        {
            const QVariant& oldValue = rawValue().value_or(QVariant());
            val = engine->fromScriptValue<QVariant>(m_mapToFunction.call(
                QJSValueList{engine->toScriptValue(value), engine->toScriptValue(oldValue)}));
        }
    }

    if (m_property.read(m_target) != val)
    {
        QSignalBlocker blocker(this);
        m_property.write(m_target, val);
    }
}

void PropertiesSyncProperty::setMapFromFunction(const QJSValue& function)
{
    m_mapFromFunction = function;
    emit mapFromFunctionChanged();
}

void PropertiesSyncProperty::setMapToFunction(const QJSValue& function)
{
    m_mapToFunction = function;
    emit mapToFunctionChanged();
}

QJSEngine* PropertiesSyncProperty::jsEngine() const
{
    return qjsEngine(this);
}

void PropertiesSyncProperty::updateProperty()
{
    if (!m_target || m_propertyName.isEmpty())
    {
        m_property = {};
        return;
    }

    const QMetaObject* metaObject = m_target->metaObject();
    const int index = metaObject->indexOfProperty(m_propertyName.toLatin1().constData());
    if (index < 0)
        m_property = {};

    m_property = m_target->metaObject()->property(index);

    if (m_property.hasNotifySignal())
    {
        const QMetaObject* metaObject = this->metaObject();
        const int index = metaObject->indexOfSignal("valueChanged()");
        if (index >= 0)
            connect(m_target, m_property.notifySignal(), this, metaObject->method(index));
    }
}

//-------------------------------------------------------------------------------------------------

PropertiesSync::PropertiesSync(QObject* parent):
    QObject(parent)
{
}

void PropertiesSync::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;

    m_enabled = enabled;

    if (m_enabled)
    {
        for (const auto& property: m_properties)
            connectToProperty(property);
    }
    else
    {
        for (const auto& property: m_properties)
            property->disconnect(this);
    }

    emit enabledChanged();
}

QQmlListProperty<PropertiesSyncProperty> PropertiesSync::synchedProperties()
{
    return QQmlListProperty<PropertiesSyncProperty>(this, nullptr,
        &appendProperty, &propertiesCount, &getProperty, &clearProperties);
}

QVariant PropertiesSync::value() const
{
    return value(nullptr).value_or(QVariant());
}

void PropertiesSync::setValue(const QVariant& value)
{
    for (const auto& property: m_properties)
        property->setValue(value);
}

void PropertiesSync::start(const QVariant& initialValue)
{
    setValue(initialValue);
    setEnabled(true);
}

void PropertiesSync::registerQmlTypes()
{
    qmlRegisterType<PropertiesSync>("nx.vms.client.core", 1, 0, "PropertiesSync");
    qmlRegisterType<PropertiesSyncProperty>("nx.vms.client.core", 1, 0, "Property");
}

void PropertiesSync::append(PropertiesSyncProperty* property)
{
    if (m_properties.contains(property))
        return;

    m_properties.append(property);
    if (m_enabled)
        connectToProperty(property);
}

int PropertiesSync::count() const
{
    return m_properties.size();
}

PropertiesSyncProperty* PropertiesSync::synchedProperty(int index) const
{
    if (index >= m_properties.size())
        return nullptr;

    return m_properties[index];
}

void PropertiesSync::clear()
{
    for (const auto& property: m_properties)
        property->disconnect(this);

    m_properties.clear();
}

void PropertiesSync::connectToProperty(PropertiesSyncProperty* property)
{
    connect(property, &PropertiesSyncProperty::valueChanged,
        this, &PropertiesSync::atValueChanged);
    connect(property, &PropertiesSyncProperty::targetChanged,
        this, &PropertiesSync::initProperty);
    connect(property, &PropertiesSyncProperty::propertyNameChanged,
        this, &PropertiesSync::initProperty);
}

void PropertiesSync::atValueChanged()
{
    const auto changedProperty = static_cast<PropertiesSyncProperty*>(sender());
    if (!changedProperty)
        return;

    if (const std::optional<QVariant>& value = changedProperty->value())
    {
        for (const auto& property: m_properties)
        {
            if (property != changedProperty)
                property->setValue(*value);
        }
    }
}

void PropertiesSync::initProperty()
{
    if (!m_initNewProperties)
        return;

    const auto changedProperty = static_cast<PropertiesSyncProperty*>(sender());
    if (!changedProperty || !changedProperty->isValid())
        return;

    if (const std::optional<QVariant>& value = this->value(changedProperty))
        changedProperty->setValue(*value);
}

std::optional<QVariant> PropertiesSync::value(PropertiesSyncProperty* ignoredProperty) const
{
    if (!m_properties.isEmpty())
    {
        for (const auto& property: m_properties)
        {
            if (property != ignoredProperty && property->isValid())
            {
                if (const std::optional<QVariant>& value = property->value())
                    return *value;
            }
        }
    }

    return {};
}

} // namespace nx::vms::client::core
