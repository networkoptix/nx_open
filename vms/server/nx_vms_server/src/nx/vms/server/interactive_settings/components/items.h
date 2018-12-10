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

class Group: public Item
{
    Q_OBJECT
    Q_PROPERTY(
        // Qt MOC requires full namespace to make the property functional.
        QQmlListProperty<nx::vms::server::interactive_settings::components::Item> items
        READ items)
    Q_CLASSINFO("DefaultProperty", "items")

    using base_type = Item;

public:
    using Item::Item;

    QQmlListProperty<Item> items();
    const QList<Item*> itemsList() const;

    virtual QJsonObject serialize() const override;

private:
    QList<Item*> m_items;
};

class ValueItem: public Item
{
    Q_OBJECT
    Q_PROPERTY(QVariant value READ value WRITE setValue NOTIFY valueChanged)
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

class Settings: public Group
{
    Q_OBJECT

public:
    Settings(QObject* parent = nullptr);
};

class GroupBox: public Group
{
public:
    GroupBox(QObject* parent = nullptr);
};

class Section: public Group
{
public:
    Section(QObject* parent = nullptr);
};

class Row: public Group
{
public:
    Row(QObject* parent = nullptr);
};

class TextField: public ValueItem
{
public:
    TextField(QObject* parent = nullptr);
};

class ComboBox: public EnumerationItem
{
public:
    ComboBox(QObject* parent = nullptr);
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
// Items factory.

class Factory
{
public:
    static void registerTypes();
    static Item* createItem(const QString &type, QObject* parent = nullptr);
};

} // namespace nx::vms::server::interactive_settings::components
