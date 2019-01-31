#include <gtest/gtest.h>

#include <QtCore/QUrl>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>
#include <QtCore/QFile>

#include <nx/vms/server/interactive_settings/qml_engine.h>
#include <nx/vms/server/interactive_settings/json_engine.h>
#include <nx/vms/server/interactive_settings/components/items.h>

static void PrintTo(const QJsonObject& object, ::std::ostream* os)
{
    *os << QJsonDocument(object).toJson().toStdString();
}

static void PrintTo(const QVariantMap& map, ::std::ostream* os)
{
    *os << QJsonDocument(QJsonObject::fromVariantMap(map)).toJson().toStdString();
}

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

} // namespace

TEST(InteractiveSettings, simpleInstantiation)
{
    TestQmlEngine engine;
    const auto result = engine.loadModelFromFile(kTestDataPath + "SimpleInstantiation.qml");
    ASSERT_EQ(result.code, QmlEngine::ErrorCode::ok);
}

TEST(InteractiveSettings, nonSettingsRootItem)
{
    TestQmlEngine engine;
    const auto result = engine.loadModelFromFile(kTestDataPath + "NonSettingsRootItem.qml");
    ASSERT_EQ(result.code, QmlEngine::ErrorCode::parseError);
}

TEST(InteractiveSettings, duplicateNames)
{
    TestQmlEngine engine;
    const auto result = engine.loadModelFromFile(kTestDataPath + "DuplicateNames.qml");
    ASSERT_EQ(result.code, QmlEngine::ErrorCode::itemNameIsNotUnique);
}

TEST(InteractiveSettings, simpleSerialization)
{
    TestQmlEngine engine;
    const auto result = engine.loadModelFromFile(kTestDataPath + "SimpleSerialization.qml");
    ASSERT_EQ(result.code, QmlEngine::ErrorCode::ok);

    const auto actualModel = engine.serializeModel();
    const auto expectedModel = QJsonObject::fromVariantMap(
        engine.settingsItem()->property(kSerializedModelProperty).toMap());
    ASSERT_EQ(actualModel, expectedModel);

    const auto actualValues = engine.values();
    const auto expectedValues = engine.settingsItem()->property(kValuesProperty).toMap();
    ASSERT_EQ(actualValues, expectedValues);
}

TEST(InteractiveSettings, simpleJsonSerialization)
{
    const auto testFileName = kLocalTestDataPath + "simple_serialization.json";

    JsonEngine engine;
    const auto result = engine.loadModelFromFile(testFileName);
    ASSERT_EQ(result.code, QmlEngine::ErrorCode::ok);

    const auto actualModel = engine.serializeModel();
    const auto expectedModel = loadJsonFromFile(testFileName);
    ASSERT_EQ(actualModel, expectedModel);
}

TEST(InteractiveSettings, valuesSerialization)
{
    TestQmlEngine engine;
    const auto result = engine.loadModelFromFile(kTestDataPath + "SimpleSerialization.qml");
    ASSERT_EQ(result.code, QmlEngine::ErrorCode::ok);

    const auto actualValues = engine.values();
    const auto expectedValues = engine.settingsItem()->property(kValuesProperty).toMap();

    ASSERT_EQ(actualValues, expectedValues);
}

TEST(InteractiveSettings, dependentProperty)
{
    TestQmlEngine engine;
    const auto result = engine.loadModelFromFile(kTestDataPath + "DependentProperty.qml");
    ASSERT_EQ(result.code, QmlEngine::ErrorCode::ok);

    const auto actualModel = engine.serializeModel();
    const auto expectedModel = QJsonObject::fromVariantMap(
        engine.settingsItem()->property(kSerializedModelProperty).toMap());
    ASSERT_EQ(actualModel, expectedModel);

    engine.applyValues(QVariantMap{{"master", true}});

    const auto changedModel = engine.serializeModel();
    const auto expectedChangedModel = QJsonObject::fromVariantMap(
        engine.settingsItem()->property(kSerializedModelProperty).toMap());
    ASSERT_EQ(changedModel, expectedChangedModel);

    ASSERT_NE(actualModel, changedModel);
}

TEST(InteractiveSettings, tryValues)
{
    TestQmlEngine engine;
    const auto result = engine.loadModelFromFile(kTestDataPath + "SimpleSerialization.qml");
    ASSERT_EQ(result.code, QmlEngine::ErrorCode::ok);

    const auto originalValues = engine.values();
    const auto [model, changedValues] = engine.tryValues(QVariantMap{{"number", 32}});
    const auto restoredValues = engine.values();

    ASSERT_NE(originalValues, changedValues);
    ASSERT_EQ(originalValues, restoredValues);
}

TEST(InteractiveSettings, rangeCheck)
{
    TestQmlEngine engine;
    const auto result = engine.loadModelFromFile(kTestDataPath + "RangeCheck.qml");
    ASSERT_EQ(result.code, QmlEngine::ErrorCode::ok);

    const auto testValues = engine.settingsItem()->property(kTestValuesProperty).toMap();
    engine.applyValues(testValues);

    const auto actualValues = engine.values();
    const auto expectedValues = engine.settingsItem()->property(kValuesProperty).toMap();

    ASSERT_EQ(actualValues, expectedValues);
}

} // namespace nx::vms::server::interactive_settings::test
