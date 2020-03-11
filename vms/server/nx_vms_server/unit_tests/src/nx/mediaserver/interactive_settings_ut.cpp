#include <gtest/gtest.h>

#include <QtCore/QUrl>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QFile>

#include <nx/vms/server/interactive_settings/qml_engine.h>
#include <nx/vms/server/interactive_settings/json_engine.h>
#include <nx/vms/server/interactive_settings/components/section.h>
#include <nx/vms/server/interactive_settings/components/value_item.h>

static void PrintTo(const QJsonValue& value, ::std::ostream* os)
{
    *os << nx::vms::server::interactive_settings::components::ValueItem::serializeJsonValue(value)
        .toStdString();
}

namespace nx::vms::server::interactive_settings {

static void PrintTo(const Issue::Code& code, ::std::ostream* os)
{
    *os << toString(code).toStdString();
}

} // namespace nx::vms::server::interactive_settings

namespace nx::vms::server::interactive_settings::test {

namespace {

static const QString kTestDataPath = "qrc:///interactive_settings/";
static const QString kLocalTestDataPath = ":/interactive_settings/";
static const char* kSerializedModelProperty = "_serializedModel";
static const char* kValuesProperty = "_values";
static const char* kTestValuesProperty = "_testValues";

QJsonObject loadJsonFromFile(const QString& fileName)
{
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly))
        return {};

    return QJsonDocument::fromJson(file.readAll()).object();
}

class TestQmlEngine: public QmlEngine
{
public:
    using QmlEngine::settingsItem;
};

class TestJsonEngine: public JsonEngine
{
public:
    using JsonEngine::settingsItem;
};

} // namespace

TEST(InteractiveSettings, simpleInstantiation)
{
    TestJsonEngine engine;
    ASSERT_TRUE(engine.loadModelFromData(R"JSON({"type": "Settings"})JSON"));
}

TEST(InteractiveSettings, invalidRootItem)
{
    TestJsonEngine engine;
    ASSERT_FALSE(engine.loadModelFromData(R"JSON({"type": "GroupBox"})JSON"));
    ASSERT_TRUE(engine.hasErrors());
    ASSERT_EQ(engine.issues().first().code, Issue::Code::parseError);
}

TEST(InteractiveSettings, duplicateNames)
{
    const auto model = R"JSON(
        {
            "type": "Settings",
            "items": [
                {"type": "TextField", "name": "text"},
                {"type": "TextArea", "name": "text"}
            ]
        })JSON";

    TestJsonEngine engine;
    ASSERT_FALSE(engine.loadModelFromData(model));
    ASSERT_TRUE(engine.hasErrors());
    ASSERT_EQ(engine.issues().first().code, Issue::Code::itemNameIsNotUnique);
}

TEST(InteractiveSettings, simpleSerialization)
{
    TestQmlEngine engine;
    ASSERT_TRUE(engine.loadModelFromFile(kTestDataPath + "SimpleSerialization.qml"));

    const auto actualModel = engine.serializeModel();
    const auto expectedModel = QJsonObject::fromVariantMap(
        engine.settingsItem()->property(kSerializedModelProperty).toMap());
    ASSERT_EQ(actualModel, expectedModel);

    const auto actualValues = engine.values();
    const auto expectedValues = QJsonObject::fromVariantMap(
        engine.settingsItem()->property(kValuesProperty).toMap());
    ASSERT_EQ(actualValues, expectedValues);
}

TEST(InteractiveSettings, simpleJsonSerialization)
{
    const auto testFileName = kLocalTestDataPath + "simple_serialization.json";

    TestJsonEngine engine;
    ASSERT_TRUE(engine.loadModelFromFile(testFileName));

    const auto actualModel = engine.serializeModel();
    const auto expectedModel = loadJsonFromFile(testFileName);
    ASSERT_EQ(actualModel, expectedModel);
}

TEST(InteractiveSettings, valuesSerialization)
{
    TestQmlEngine engine;
    ASSERT_TRUE(engine.loadModelFromFile(kTestDataPath + "SimpleSerialization.qml"));

    const auto actualValues = engine.values();
    const auto expectedValues = QJsonObject::fromVariantMap(
        engine.settingsItem()->property(kValuesProperty).toMap());

    ASSERT_EQ(actualValues, expectedValues);
}

TEST(InteractiveSettings, dependentProperty)
{
    TestQmlEngine engine;
    ASSERT_TRUE(engine.loadModelFromFile(kTestDataPath + "DependentProperty.qml"));

    const auto actualModel = engine.serializeModel();
    const auto expectedModel = QJsonObject::fromVariantMap(
        engine.settingsItem()->property(kSerializedModelProperty).toMap());
    ASSERT_EQ(actualModel, expectedModel);

    engine.applyValues(QJsonObject{{"master", true}});

    const auto changedModel = engine.serializeModel();
    const auto expectedChangedModel = QJsonObject::fromVariantMap(
        engine.settingsItem()->property(kSerializedModelProperty).toMap());
    ASSERT_EQ(changedModel, expectedChangedModel);

    ASSERT_NE(actualModel, changedModel);
}

TEST(InteractiveSettings, tryValues)
{
    TestQmlEngine engine;
    ASSERT_TRUE(engine.loadModelFromFile(kTestDataPath + "SimpleSerialization.qml"));

    const auto originalValues = engine.values();
    const auto [model, changedValues] = engine.tryValues(QJsonObject{{"number", 32}});
    const auto restoredValues = engine.values();

    ASSERT_NE(originalValues, changedValues);
    ASSERT_EQ(originalValues, restoredValues);
}

TEST(InteractiveSettings, stringValues)
{
    TestQmlEngine engine;
    ASSERT_TRUE(engine.loadModelFromFile(kTestDataPath + "SimpleSerialization.qml"));

    const auto stringValues = QJsonObject::fromVariantMap(
        engine.settingsItem()->property("_stringValues").toMap());

    engine.applyStringValues(stringValues);
    ASSERT_EQ(engine.issues().size(), 0);

    const auto expectedValues = QJsonObject::fromVariantMap(
        engine.settingsItem()->property("_resultValues").toMap());
    const auto actualValues = engine.values();
    ASSERT_EQ(actualValues, expectedValues);
}

TEST(InteractiveSettings, rangeCheck)
{
    TestQmlEngine engine;
    ASSERT_TRUE(engine.loadModelFromFile(kTestDataPath + "RangeCheck.qml"));

    const auto testValues = QJsonObject::fromVariantMap(
        engine.settingsItem()->property(kTestValuesProperty).toMap());
    engine.applyValues(testValues);

    const auto actualValues = engine.values();
    const auto expectedValues = QJsonObject::fromVariantMap(
        engine.settingsItem()->property(kValuesProperty).toMap());

    ASSERT_EQ(actualValues, expectedValues);
}

TEST(InteractiveSettings, rangeMustContainStringsOnly)
{
    {
        const auto jsonWithWarnings = R"JSON(
            {
                "type": "Settings",
                "items": [{"type": "ComboBox", "range": ["a", 1, false, null, "z"]}]
            })JSON";

        TestJsonEngine engine;
        ASSERT_TRUE(engine.loadModelFromData(jsonWithWarnings));
        ASSERT_EQ(engine.issues().size(), 3);
    }

    {
        const auto jsonWithError= R"JSON(
            {
                "type": "Settings",
                "items": [{"type": "ComboBox", "range": ["a", {}, "z"]}]
            })JSON";

        TestJsonEngine engine;
        ASSERT_FALSE(engine.loadModelFromData(jsonWithError));
        ASSERT_EQ(engine.issues().size(), 1);
    }
}

TEST(InteractiveSettings, constraintsMuFstBeAppliedAfterValues)
{
    TestQmlEngine engine;
    ASSERT_TRUE(engine.loadModelFromFile(kTestDataPath + "Constraints.qml"));

    const auto _impossibleValues = QJsonObject::fromVariantMap(
        engine.settingsItem()->property("_impossibleValues").toMap());
    engine.applyValues(_impossibleValues);

    auto actualValues = engine.values();
    ASSERT_NE(actualValues, _impossibleValues);

    const auto _correctValues = QJsonObject::fromVariantMap(
        engine.settingsItem()->property("_correctValues").toMap());
    engine.applyValues(_correctValues);

    actualValues = engine.values();
    ASSERT_EQ(actualValues, _correctValues);
}

TEST(InteractiveSettings, repeater)
{
    const auto testFileName = kLocalTestDataPath + "repeater.json";

    TestJsonEngine engine;
    ASSERT_TRUE(engine.loadModelFromFile(testFileName));

    const auto actualValues = engine.values();
    const auto expectedValues = loadJsonFromFile(testFileName)["_values"].toObject();

    ASSERT_EQ(actualValues, expectedValues);
}

const auto valuesCheckModel = R"JSON(
    {
        "type": "Settings",
        "items": [
            {"type": "TextField", "name": "string"},
            {"type": "SpinBox", "name": "number"},
            {"type": "CheckBox", "name": "bool"},
            {"type": "ComboBox", "name": "enum", "range": ["a", "b", "c"]},
            {"type": "CheckBoxGroup", "name": "array", "range": ["a", "b", "c"]},
            {"type": "BoxFigure", "name": "object"}
        ]
    })JSON";

TEST(InteractiveSettings, stringValuesAssignment)
{
    TestJsonEngine engine;
    ASSERT_TRUE(engine.loadModelFromData(valuesCheckModel));

    const auto& setValue =
        [&](const QString& name, const QJsonValue& value)
        {
            engine.applyStringValues({{name, value}});
        };

    const auto& value =
        [&](const QString& name) -> QJsonValue { return engine.values().value(name); };

    setValue("string", "a");
    ASSERT_EQ(engine.issues().size(), 0);
    ASSERT_EQ(value("string"), "a");

    setValue("number", "42");
    ASSERT_EQ(engine.issues().size(), 0);
    ASSERT_EQ(value("number"), 42);

    setValue("number", "a");
    ASSERT_EQ(engine.issues().size(), 1);
    ASSERT_EQ(value("number"), 0);

    setValue("bool", "true");
    ASSERT_EQ(engine.issues().size(), 0);
    ASSERT_EQ(value("bool"), true);

    setValue("bool", "fail");
    ASSERT_EQ(engine.issues().size(), 1);
    ASSERT_EQ(value("bool"), false);

    setValue("enum", "c");
    ASSERT_EQ(engine.issues().size(), 0);
    ASSERT_EQ(value("enum"), "c");

    setValue("enum", "x");
    ASSERT_EQ(engine.issues().size(), 1);
    ASSERT_EQ(value("enum"), "a");

    setValue("array", "[\"c\"]");
    ASSERT_EQ(engine.issues().size(), 0);
    ASSERT_EQ(value("array"), (QJsonArray{"c"}));

    // String must contain an array to be converted without warnings.
    setValue("array", "c");
    ASSERT_EQ(engine.issues().size(), 1);
    ASSERT_EQ(value("array"), (QJsonArray{"c"}));

    setValue("object", "{\"a\": 1}");
    ASSERT_EQ(engine.issues().size(), 0);
    ASSERT_EQ(value("object"), (QJsonObject{{"a", 1}}));
}

TEST(InteractiveSettings, omittedDefaultValueIsNotAnIssue)
{
    TestJsonEngine engine;
    ASSERT_TRUE(engine.loadModelFromData(valuesCheckModel));
    ASSERT_EQ(engine.issues().size(), 0);
}

TEST(InteractiveSettings, defaultValueMustBeCorrect)
{
    const auto model = R"JSON(
        {
            "type": "Settings",
            "items": [{"type": "ComboBox", "range": ["a", "b", "c"], "defaultValue": "x"}]
        })JSON";

    TestJsonEngine engine;
    ASSERT_FALSE(engine.loadModelFromData(model));
    ASSERT_TRUE(engine.hasErrors());
    ASSERT_EQ(engine.issues().first().code, Issue::Code::cannotConvertValue);
}

TEST(InteractiveSettings, defaultValueTypeShouldBeCorrect)
{
    const auto model = R"JSON(
       {
           "type": "Settings",
           "items": [{"type": "SpinBox", "defaultValue": "10"}]
       })JSON";

    TestJsonEngine engine;
    ASSERT_TRUE(engine.loadModelFromData(model));
    ASSERT_FALSE(engine.hasErrors());
    ASSERT_GE(engine.issues().size(), 1);
    ASSERT_EQ(engine.issues().first().code, Issue::Code::valueConverted);
}

} // namespace nx::vms::server::interactive_settings::test
