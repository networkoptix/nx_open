#include "items.h"

#include <QtCore/QJsonArray>
#include <QtQml/QtQml>

#include <nx/utils/thread/mutex.h>

namespace nx::vms::server::interactive_settings::components {

Item::Item(const QString& type, QObject* parent) :
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

Separator::Separator(QObject* parent):
    Item(QStringLiteral("Separator"), parent)
{
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
    for (const auto item : m_items)
        items.append(item->serialize());
    result[QStringLiteral("items")] = items;

    return result;
}

Repeater::Repeater(QObject* parent):
    base_type(QStringLiteral("Repeater"), parent)
{
}

QVariant Repeater::itemTemplate() const
{
    return m_itemTemplate.toVariantMap();
}

void Repeater::setItemTemplate(const QVariant& itemTemplate)
{
    const auto templateJson = QJsonObject::fromVariantMap(itemTemplate.toMap());
    if (m_itemTemplate == templateJson)
        return;

    m_itemTemplate = templateJson;
    emit itemTemplateChanged();
}

void Repeater::setCount(int count)
{
    if (m_count == count)
        return;

    m_count = count;
    emit countChanged();
}

void Repeater::setStartIndex(int index)
{
    if (m_startIndex == index)
        return;

    m_startIndex = index;
    emit startIndexChanged();
}

void Repeater::setAddButtonCaption(const QString& caption)
{
    if (m_addButtonCaption == caption)
        return;

    m_addButtonCaption = caption;
    emit addButtonCaptionChanged();
}

QJsonObject Repeater::serialize() const
{
    auto result = base_type::serialize();
    result[QStringLiteral("addButtonCaption")] = m_addButtonCaption;
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

QQmlListProperty<Section> SectionContainer::sections()
{
    return QQmlListProperty<Section>(this, m_sections);
}

const QList<Section*> SectionContainer::sectionList() const
{
    return m_sections;
}

QJsonObject SectionContainer::serialize() const
{
    auto result = base_type::serialize();
    if (m_sections.empty())
        return result;

    QJsonArray sections;
    for (const auto section: m_sections)
        sections.append(section->serialize());

    result[QStringLiteral("sections")] = sections;
    return result;
}

Section::Section(QObject* parent):
    base_type(QStringLiteral("Section"), parent)
{
}

Settings::Settings(QObject* parent):
    base_type(QStringLiteral("Settings"), parent)
{
}

GroupBox::GroupBox(QObject* parent):
    Group(QStringLiteral("GroupBox"), parent)
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

QString TextField::validationRegex() const
{
    return m_validationRegex;
}

void TextField::setValidationRegex(const QString& regex)
{
    if (m_validationRegex == regex)
        return;

    m_validationRegex = regex;
    emit validationRegexChanged();
}

QString TextField::validationRegexFlags() const
{
    return m_validationRegexFlags;
}

void TextField::setValidationRegexFlags(const QString& flags)
{
    if (m_validationRegexFlags == flags)
        return;

    m_validationRegexFlags = flags;
    emit validationRegexFlagsChanged();
}

QString TextField::validationErrorMessage() const
{
    return m_validationErrorMessage;
}

void TextField::setValidationErrorMessage(const QString& message)
{
    if (m_validationErrorMessage == message)
        return;

    m_validationErrorMessage = message;
    emit validationErrorMessageChanged();
}

QJsonObject TextField::serialize() const
{
    auto result = base_type::serialize();
    result[QStringLiteral("validationRegex")] = validationRegex();
    result[QStringLiteral("validationRegexFlags")] = validationRegexFlags();
    result[QStringLiteral("validationErrorMessage")] = validationErrorMessage();
    return result;
}

TextArea::TextArea(QObject* parent):
    ValueItem(QStringLiteral("TextArea"), parent)
{
}

ComboBox::ComboBox(QObject* parent):
    EnumerationItem(QStringLiteral("ComboBox"), parent)
{
}

RadioButtonGroup::RadioButtonGroup(QObject* parent):
    EnumerationItem(QStringLiteral("RadioButtonGroup"), parent)
{
}

CheckBoxGroup::CheckBoxGroup(QObject* parent):
    EnumerationItem(QStringLiteral("CheckBoxGroup"), parent)
{
}

void CheckBoxGroup::setValue(const QVariant& value)
{
    const auto validOptions = range();
    QVariantList effectiveValue;

    for (const auto& option: value.value<QVariantList>())
    {
        if (validOptions.contains(option))
            effectiveValue.push_back(option);
    }

    ValueItem::setValue(effectiveValue);
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
// ROI components.

BaseFigure::BaseFigure(const QString& figureType, QObject* parent):
    ValueItem(figureType, parent)
{
}

/*static*/ QJsonObject BaseFigure::mergeFigures(
    const QJsonObject& currentFigureValue, const QJsonObject& newFigureValue)
{
    QJsonObject result = currentFigureValue;
    for (auto it = newFigureValue.begin(); it != newFigureValue.end(); ++it)
    {
        const QString propertyKey = it.key();

        if ((it->isNull() || it->isUndefined()) && !kNullableFields.contains(propertyKey))
            continue;

        if (it->isObject())
        {
            result[propertyKey] =
                mergeFigures(result[propertyKey].toObject(), it.value().toObject());
        }
        else
        {
            result[propertyKey] = it.value();
        }
    }

    return result;
}

void BaseFigure::setValue(const QVariant& value)
{
    QJsonObject valueJsonObject;
    if (value.canConvert<std::nullptr_t>()) //< Must be handled first.
    {
        m_value.setValue(nullptr);
        return;
    }
    else if (value.canConvert<QString>())
    {
        QJsonDocument jsonDocument = QJsonDocument::fromJson(value.toString().toUtf8());
        if (jsonDocument.isObject())
            valueJsonObject = jsonDocument.object();
        else
            return;
    }
    else if (value.canConvert<QVariantMap>())
    {
        valueJsonObject = QJsonValue::fromVariant(value).toObject();
    }
    else
    {
        return;
    }

    m_value.setValue(mergeFigures(QJsonValue::fromVariant(m_value).toObject(), valueJsonObject));
}

LineFigure::LineFigure(QObject* parent):
    BaseFigure(QStringLiteral("LineFigure"), parent)
{
}

PolygonFigure::PolygonFigure(QObject* parent):
    BaseFigure(QStringLiteral("BoxFigure"), parent)
{
}

void PolygonFigure::setMaxPoints(int maxPoints)
{
    if (m_maxPoints == maxPoints)
        return;

    m_maxPoints = maxPoints;
    emit maxPointsChanged();
}

BoxFigure::BoxFigure(QObject* parent):
    BaseFigure(QStringLiteral("PolygonFigure"), parent)
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
    registerType<Repeater>("Repeater");
    registerType<Row>("Row");
    registerType<TextField>("TextField");
    registerType<TextArea>("TextArea");
    registerType<ComboBox>("ComboBox");
    registerType<SpinBox>("SpinBox");
    registerType<DoubleSpinBox>("DoubleSpinBox");
    registerType<CheckBox>("CheckBox");
    registerType<Button>("Button");
    registerType<Separator>("Separator");
    registerType<RadioButtonGroup>("RadioButtonGroup");
    registerType<CheckBoxGroup>("CheckBoxGroup");

    registerType<LineFigure>("LineFigure");
    registerType<BoxFigure>("BoxFigure");
    registerType<PolygonFigure>("PolygonFigure");
}

Item* Factory::createItem(const QString& type, QObject* parent)
{
    if (auto instantiator = itemInstantiators.value(type))
        return instantiator(parent);
    return nullptr;
}

} // namespace nx::vms::server::interactive_settings::components
