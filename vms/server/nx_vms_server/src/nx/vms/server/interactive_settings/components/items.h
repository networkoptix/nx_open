#pragma once

#include <memory>

#include <QtCore/QObject>
#include <QtCore/QJsonObject>
#include <QtQml/QQmlListProperty>

namespace nx::vms::server::interactive_settings::components {

class Item: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString type MEMBER m_type FINAL CONSTANT)
    Q_PROPERTY(QString name MEMBER m_name FINAL)
    Q_PROPERTY(QString caption MEMBER m_caption FINAL)
    Q_PROPERTY(QString description MEMBER m_description FINAL)

public:
    QString type() const
    {
        return m_type;
    }

    QString name() const
    {
        return m_name;
    }

    QString caption() const
    {
        return m_caption;
    }

    QString description() const
    {
        return m_description;
    }

    virtual QJsonObject serialize() const;

protected:
    Item(const QString& type, QObject* parent = nullptr);

private:
    const QString m_type;
    QString m_name;
    QString m_caption;
    QString m_description;
};

class Separator: public Item
{
    Q_OBJECT

public:
    Separator(QObject* parent = nullptr);
};

class Group: public Item
{
    Q_OBJECT
    Q_PROPERTY(
        // Qt MOC requires full namespace to make the property functional.
        QQmlListProperty<nx::vms::server::interactive_settings::components::Item> items
        READ items)
    Q_PROPERTY(QVariantList filledCheckItems READ filledCheckItems WRITE setFilledCheckItems
        NOTIFY filledCheckItemsChanged)
    Q_CLASSINFO("DefaultProperty", "items")

    using base_type = Item;

public:
    using Item::Item;

    QQmlListProperty<Item> items();
    const QList<Item*> itemsList() const;

    QVariantList filledCheckItems() const { return m_filledCheckItems; }
    void setFilledCheckItems(const QVariantList& items);

    virtual QJsonObject serialize() const override;

signals:
    void filledCheckItemsChanged();

private:
    QList<Item*> m_items;
    QVariantList m_filledCheckItems;
};

class Repeater: public Group
{
    Q_OBJECT
    Q_PROPERTY(QVariant template READ itemTemplate WRITE setItemTemplate
        NOTIFY itemTemplateChanged)
    Q_PROPERTY(int count READ count WRITE setCount NOTIFY countChanged)
    Q_PROPERTY(int startIndex READ startIndex WRITE setStartIndex NOTIFY startIndexChanged)
    Q_PROPERTY(QString addButtonCaption READ addButtonCaption WRITE setAddButtonCaption
        NOTIFY addButtonCaptionChanged)

    using base_type = Group;

public:
    Repeater(QObject* parent = nullptr);

    QVariant itemTemplate() const;
    void setItemTemplate(const QVariant& itemTemplate);

    int count() const { return m_count; }
    void setCount(int count);

    int startIndex() const { return m_startIndex; }
    void setStartIndex(int index);

    QString addButtonCaption() const { return m_addButtonCaption; }
    void setAddButtonCaption(const QString& caption);

    virtual QJsonObject serialize() const override;

signals:
    void itemTemplateChanged();
    void countChanged();
    void startIndexChanged();
    void addButtonCaptionChanged();

private:
    QJsonObject m_itemTemplate;
    int m_count = 0;
    int m_startIndex = 1;
    QString m_addButtonCaption;
};

class ValueItem: public Item
{
    Q_OBJECT
    Q_PROPERTY(QVariant value READ value NOTIFY valueChanged)
    Q_PROPERTY(QVariant defaultValue READ defaultValue WRITE setDefaultValue
        NOTIFY defaultValueChanged)

    using base_type = Item;

public:
    using Item::Item;

    QVariant value() const;

    virtual void setValue(const QVariant& value);

    QVariant defaultValue() const
    {
        return m_defaultValue;
    }

    void setDefaultValue(const QVariant& defaultValue);

    virtual QJsonObject serialize() const override;

signals:
    void valueChanged();
    void defaultValueChanged();

protected:
    QVariant m_value;
    QVariant m_defaultValue;
};

class EnumerationItem: public ValueItem
{
    Q_OBJECT
    Q_PROPERTY(QVariantList range READ range WRITE setRange NOTIFY rangeChanged)

    using base_type = ValueItem;

public:
    using ValueItem::ValueItem;

    virtual void setValue(const QVariant& value) override;

    QVariantList range() const
    {
        return m_range;
    }

    void setRange(const QVariantList& range);

    virtual QJsonObject serialize() const override;

signals:
    void rangeChanged();

protected:
    QVariantList m_range;
};

class IntegerNumberItem: public ValueItem
{
    Q_OBJECT
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged FINAL)
    Q_PROPERTY(int minValue READ minValue WRITE setMinValue NOTIFY minValueChanged FINAL)
    Q_PROPERTY(int maxValue READ maxValue WRITE setMaxValue NOTIFY maxValueChanged FINAL)

    using base_type = ValueItem;

public:
    using ValueItem::ValueItem;

    int value() const
    {
        return ValueItem::value().toInt();
    }

    void setValue(int value);
    virtual void setValue(const QVariant& value) override;

    int minValue() const
    {
        return m_minValue;
    }

    void setMinValue(int minValue);

    int maxValue() const
    {
        return m_maxValue;
    }

    void setMaxValue(int maxValue);

    virtual QJsonObject serialize() const override;

signals:
    void minValueChanged();
    void maxValueChanged();

protected:
    int m_minValue = std::numeric_limits<int>::min();
    int m_maxValue = std::numeric_limits<int>::max();
};

class RealNumberItem: public ValueItem
{
    Q_OBJECT
    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged FINAL)
    Q_PROPERTY(double minValue READ minValue WRITE setMinValue NOTIFY minValueChanged FINAL)
    Q_PROPERTY(double maxValue READ maxValue WRITE setMaxValue NOTIFY maxValueChanged FINAL)

    using base_type = ValueItem;

public:
    using ValueItem::ValueItem;

    double value() const
    {
        return ValueItem::value().toDouble();
    }

    void setValue(double value);
    virtual void setValue(const QVariant& value) override;

    double minValue() const
    {
        return m_minValue;
    }

    void setMinValue(double minValue);

    double maxValue() const
    {
        return m_maxValue;
    }

    void setMaxValue(double maxValue);

    virtual QJsonObject serialize() const override;

signals:
    void minValueChanged();
    void maxValueChanged();

protected:
    double m_minValue = std::numeric_limits<double>::min();
    double m_maxValue = std::numeric_limits<double>::max();
};

//-------------------------------------------------------------------------------------------------
// Specific components.

class Section;
class SectionContainer: public Group
{
    Q_OBJECT
    using base_type = Group;

public:
    using base_type::base_type; //< Forward constructors.

    QQmlListProperty<Section> sections();
    const QList<Section*> sectionList() const;

    virtual QJsonObject serialize() const override;

private:
    QList<Section*> m_sections;
};

class Section: public SectionContainer
{
    Q_OBJECT
    using base_type = SectionContainer;

public:
    Section(QObject* parent = nullptr);
};

class Settings: public SectionContainer
{
    Q_OBJECT
    using base_type = SectionContainer;

public:
    Settings(QObject* parent = nullptr);
};

class GroupBox: public Group
{
public:
    GroupBox(QObject* parent = nullptr);
};

class Row: public Group
{
public:
    Row(QObject* parent = nullptr);
};

class TextField: public ValueItem
{
    Q_OBJECT
    Q_PROPERTY(QString validationRegex READ validationRegex WRITE setValidationRegex
        NOTIFY validationRegexChanged)
    Q_PROPERTY(QString validationRegexFlags READ validationRegexFlags WRITE setValidationRegexFlags
        NOTIFY validationRegexFlagsChanged)
    Q_PROPERTY(QString validationErrorMessage READ validationErrorMessage
        WRITE setValidationErrorMessage NOTIFY validationErrorMessageChanged)

    using base_type = ValueItem;

public:
    TextField(QObject* parent = nullptr);

    /**
     * Regular expression pattern to check the value.
     * Check failure does not avoid server from saving wrong values. This regex is just for UI to
     * notify a user about wrong input.
     */
    QString validationRegex() const;
    void setValidationRegex(const QString& regex);

    /**
     * Javascript RegExp flags.
     * See RegExp documentation for details.
     */
    QString validationRegexFlags() const;
    void setValidationRegexFlags(const QString& flags);

    QString validationErrorMessage() const;
    void setValidationErrorMessage(const QString& message);

    virtual QJsonObject serialize() const override;

signals:
    void validationRegexChanged();
    void validationRegexFlagsChanged();
    void validationErrorMessageChanged();

private:
    QString m_validationRegex;
    QString m_validationRegexFlags;
    QString m_validationErrorMessage;
};

class TextArea: public ValueItem
{
public:
    TextArea(QObject* parent = nullptr);
};

class ComboBox: public EnumerationItem
{
public:
    ComboBox(QObject* parent = nullptr);
};

class RadioButtonGroup: public EnumerationItem
{
public:
    RadioButtonGroup(QObject* parent = nullptr);
};

class CheckBoxGroup: public EnumerationItem
{
public:
    CheckBoxGroup(QObject* parent = nullptr);

    virtual void setValue(const QVariant& value);
};

class SpinBox: public IntegerNumberItem
{
public:
    SpinBox(QObject* parent = nullptr);
};

class DoubleSpinBox: public RealNumberItem
{
public:
    DoubleSpinBox(QObject* parent = nullptr);
};

class CheckBox: public ValueItem
{
    Q_OBJECT
    Q_PROPERTY(bool value READ value WRITE setValue NOTIFY valueChanged FINAL)

public:
    CheckBox(QObject* parent = nullptr);

    bool value() const
    {
        return ValueItem::value().toBool();
    }

    void setValue(bool value)
    {
        ValueItem::setValue(value);
    }

    using ValueItem::setValue;
};

class Button: public Item
{
public:
    Button(QObject* parent = nullptr);
};

//-------------------------------------------------------------------------------------------------
// ROI components.

class BaseFigure: public ValueItem
{
    Q_OBJECT
public:
    BaseFigure(const QString& figureType, QObject* parent = nullptr);
    virtual void setValue(const QVariant& value) override;

private:
    static QJsonObject mergeFigures(
        const QJsonObject& currentFigureValue,
        const QJsonObject& newFigureValue);
};

class BoxFigure: public BaseFigure
{
public:
    BoxFigure(QObject* parent = nullptr);
};

class PolyFigure: public BaseFigure
{
    Q_OBJECT
    Q_PROPERTY(int minPoints READ minPoints WRITE setMinPoints NOTIFY minPointsChanged)
    Q_PROPERTY(int maxPoints READ maxPoints WRITE setMaxPoints NOTIFY maxPointsChanged)

    using base_type = BaseFigure;

public:
    using BaseFigure::BaseFigure;

    int minPoints() const { return m_minPoints; }
    void setMinPoints(int minPoints);

    int maxPoints() const { return m_maxPoints; }
    void setMaxPoints(int maxPoints);

    virtual QJsonObject serialize() const override;

signals:
    void minPointsChanged();
    void maxPointsChanged();

private:
    int m_minPoints = 0;
    int m_maxPoints = 0;
};

class LineFigure: public PolyFigure
{
    Q_OBJECT
    Q_PROPERTY(QString allowedDirections MEMBER m_allowedDirections)

public:
    LineFigure(QObject* parent = nullptr);

    virtual QJsonObject serialize() const override;

private:
    QString m_allowedDirections;
};

class PolygonFigure: public PolyFigure
{

public:
    PolygonFigure(QObject* parent = nullptr);
};

//-------------------------------------------------------------------------------------------------
// Items factory.

class Factory
{
public:
    static void registerTypes();
    static Item* createItem(const QString &type, QObject* parent = nullptr);
};

} // namespace nx::vms::server::interactive_settings::components
