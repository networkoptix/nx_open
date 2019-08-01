#include "items.h"

#include <QtCore/QJsonArray>
#include <QtQml/QtQml>

#include <nx/utils/thread/mutex.h>

namespace nx::vms::server::interactive_settings::components {

Item::Item(const QString& type, QObject* parent):
    QObject(parent),
    m_type(type)
{
}

QJsonObject Item::serialize() const
{
    QJsonObject result;
    result[QLatin1String("type")] = m_type;
    if (!m_name.isEmpty())
        result[QLatin1String("name")] = m_name;
    if (!m_caption.isEmpty())
        result[QLatin1String("caption")] = m_caption;
    if (!m_description.isEmpty())
        result[QLatin1String("description")] = m_description;
    return result;
}

QQmlListProperty<Item> Group::items()
{
    return QQmlListProperty<Item>(this, m_items);
}

const QList<Item*> Group::itemsList() const
{
    return m_items;
}

QJsonObject Group::serialize() const
{
    auto result = base_type::serialize();

    QJsonArray items;
    for (const auto item: m_items)
        items.append(item->serialize());
    result[QStringLiteral("items")] = items;

    return result;
}

QVariant ValueItem::value() const
{
    return m_value.isValid() ? m_value : m_defaultValue;
}

void ValueItem::setValue(const QVariant& value)
{
    if (m_value == value)
        return;

    m_value = value;
    emit valueChanged();
}

void ValueItem::setDefaultValue(const QVariant& defaultValue)
{
    if (!m_value.isValid())
        emit valueChanged();

    m_defaultValue = defaultValue;
    emit defaultValueChanged();
}

QJsonObject ValueItem::serialize() const
{
    auto result = base_type::serialize();
    result[QLatin1String("defaultValue")] = QJsonValue::fromVariant(m_defaultValue);
    return result;
}

void EnumerationItem::setValue(const QVariant& value)
{
    base_type::setValue(m_range.contains(value) ? value : defaultValue());
}

void EnumerationItem::setRange(const QVariantList& range)
{
    if (m_range == range)
        return;

    m_range = range;
    emit rangeChanged();

    if (!m_range.contains(value()))
        setValue(defaultValue());
}

QJsonObject EnumerationItem::serialize() const
{
    auto result = base_type::serialize();
    result[QLatin1String("range")] = QJsonArray::fromVariantList(m_range);
    return result;
}

void IntegerNumberItem::setValue(int value)
{
    ValueItem::setValue(qBound(m_minValue, value, m_maxValue));
}

void IntegerNumberItem::setValue(const QVariant& value)
{
    if (value.canConvert(QVariant::Int))
        setValue(value.toInt());
}

void IntegerNumberItem::setMinValue(int minValue)
{
    if (m_minValue == minValue)
        return;

    m_minValue = minValue;
    emit minValueChanged();

    if (m_value.isValid())
        setValue(value());
}

void IntegerNumberItem::setMaxValue(int maxValue)
{
    if (m_maxValue == maxValue)
        return;

    m_maxValue = maxValue;
    emit maxValueChanged();

    if (m_value.isValid())
        setValue(value());
}

QJsonObject IntegerNumberItem::serialize() const
{
    auto result = base_type::serialize();
    result[QLatin1String("minValue")] = m_minValue;
    result[QLatin1String("maxValue")] = m_maxValue;
    return result;
}

void RealNumberItem::setValue(double value)
{
    ValueItem::setValue(qBound(m_minValue, value, m_maxValue));
}

void RealNumberItem::setValue(const QVariant& value)
{
    if (value.canConvert(QVariant::Double))
        setValue(value.toDouble());
}

void RealNumberItem::setMinValue(double minValue)
{
    if (qFuzzyCompare(m_minValue, minValue))
        return;

    m_minValue = minValue;
    emit minValueChanged();

    if (m_value.isValid())
        setValue(value());
}

void RealNumberItem::setMaxValue(double maxValue)
{
    if (qFuzzyCompare(m_maxValue, maxValue))
        return;

    m_maxValue = maxValue;
    emit maxValueChanged();

    if (m_value.isValid())
        setValue(value());
}

QJsonObject RealNumberItem::serialize() const
{
    auto result = base_type::serialize();
    result[QLatin1String("minValue")] = m_minValue;
    result[QLatin1String("maxValue")] = m_maxValue;
    return result;
}

//-------------------------------------------------------------------------------------------------
// Specific components.

Settings::Settings(QObject* parent):
    Group(QStringLiteral("Settings"), parent)
{
}

GroupBox::GroupBox(QObject* parent):
    Group(QStringLiteral("GroupBox"), parent)
{
}

Section::Section(QObject* parent):
    Group(QStringLiteral("Section"), parent)
{
}

Row::Row(QObject* parent):
    Group(QStringLiteral("Row"), parent)
{
}

TextField::TextField(QObject* parent):
    ValueItem(QStringLiteral("TextField"), parent)
{
}

TextArea::TextArea(QObject* parent):
    ValueItem(QStringLiteral("TextArea"), parent)
{
}

ComboBox::ComboBox(QObject* parent):
    EnumerationItem(QStringLiteral("ComboBox"), parent)
{
}

SpinBox::SpinBox(QObject* parent):
    IntegerNumberItem(QStringLiteral("SpinBox"), parent)
{
}

DoubleSpinBox::DoubleSpinBox(QObject* parent):
    RealNumberItem(QStringLiteral("DoubleSpinBox"), parent)
{
}

CheckBox::CheckBox(QObject* parent):
    ValueItem(QStringLiteral("CheckBox"), parent)
{
}

Button::Button(QObject* parent):
    Item(QStringLiteral("Button"), parent)
{
}

//-------------------------------------------------------------------------------------------------
// Items factory.

namespace {

using ItemInstantiator = std::function<Item*(QObject*)>;
QHash<QString, ItemInstantiator> itemInstantiators;
const char* kUri = "nx.mediaserver.interactive_settings";
utils::Mutex mutex;

template<typename T>
void registerType(const char* name)
{
    qmlRegisterType<T>(kUri, 1, 0, name);
    itemInstantiators.insert(name, [](QObject* parent) { return new T(parent); });
}

template<typename T>
void registerUncreatableType(const char* name)
{
    qmlRegisterUncreatableType<T>(kUri, 1, 0, name,
        QStringLiteral("Cannot create object of type %1").arg(name));
}

} // namespace

void Factory::registerTypes()
{
    NX_MUTEX_LOCKER lock(&mutex);

    if (!itemInstantiators.isEmpty())
        return;

    registerUncreatableType<Item>("Item");
    registerUncreatableType<ValueItem>("ValueItem");
    registerUncreatableType<EnumerationItem>("EnumerationItem");
    registerUncreatableType<IntegerNumberItem>("IntegerNumberItem");

    registerType<Settings>("Settings");
    registerType<GroupBox>("GroupBox");
    registerType<Section>("Section");
    registerType<Row>("Row");
    registerType<TextField>("TextField");
    registerType<TextArea>("TextArea");
    registerType<ComboBox>("ComboBox");
    registerType<SpinBox>("SpinBox");
    registerType<DoubleSpinBox>("DoubleSpinBox");
    registerType<CheckBox>("CheckBox");
    registerType<Button>("Button");
}

Item* Factory::createItem(const QString& type, QObject* parent)
{
    if (auto instantiator = itemInstantiators.value(type))
        return instantiator(parent);
    return nullptr;
}

} // namespace nx::vms::server::interactive_settings::components
