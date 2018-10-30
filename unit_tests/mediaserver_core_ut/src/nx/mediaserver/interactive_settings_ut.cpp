#include <gtest/gtest.h>

#include <QtCore/QUrl>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>
#include <QtCore/QFile>

#include <nx/mediaserver/interactive_settings/qml_engine.h>
#include <nx/mediaserver/interactive_settings/json_engine.h>

static void PrintTo(const QJsonObject& object, ::std::ostream* os)
{
    *os << QJsonDocument(object).toJson().toStdString();
}

static void PrintTo(const QVariantMap& map, ::std::ostream* os)
{
    *os << QJsonDocument(QJsonObject::fromVariantMap(map)).toJson().toStdString();
}

namespace nx::mediaserver::interactive_settings::test {

namespace {

static const QString kTestDataPath = "qrc:///interactive_settings/";
static const QString kLocalTestDataPath = ":/interactive_settings/";
static const char* kSerializedProperty = "_serialized";
static const char* kValuesProperty = "_values";
static const char* kTestValuesProperty = "_testValues";

QJsonObject loadJsonFromFile(const QString& fileName)
{
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly))
        return {};

    return QJsonDocument::fromJson(file.readAll()).object();
}

} // namespace

TEST(InteractiveSettings, simpleInstantiation)
{
    QmlEngine engine;
    engine.load(kTestDataPath + "SimpleInstantiation.qml");
    ASSERT_EQ(engine.status(), QmlEngine::Status::loaded);
}

TEST(InteractiveSettings, nonSettingsRootItem)
{
    QmlEngine engine;
    // This will produce an error message.
    engine.load(kTestDataPath + "NonSettingsRootItem.qml");
    ASSERT_EQ(engine.status(), QmlEngine::Status::error);
}

TEST(InteractiveSettings, duplicateNames)
{
    QmlEngine engine;
    // This will produce an error message.
    engine.load(kTestDataPath + "DuplicateNames.qml");
    ASSERT_EQ(engine.status(), QmlEngine::Status::error);
}

TEST(InteractiveSettings, simpleSerialization)
{
    QmlEngine engine;
    engine.load(kTestDataPath + "SimpleSerialization.qml");
    ASSERT_EQ(engine.status(), QmlEngine::Status::loaded);

    const auto settingsData = engine.serialize();
    const auto expectedData = QJsonObject::fromVariantMap(
        engine.rootObject()->property(kSerializedProperty).toMap());

    ASSERT_EQ(settingsData, expectedData);
}

TEST(InteractiveSettings, simpleJsonSerialization)
{
    const auto testFileName = kLocalTestDataPath + "simple_serialization.json";

    JsonEngine engine;
    engine.load(testFileName);
    ASSERT_EQ(engine.status(), JsonEngine::Status::loaded);

    const auto settingsData = engine.serialize();
    const auto expectedData = loadJsonFromFile(testFileName);

    ASSERT_EQ(settingsData, expectedData);
}

TEST(InteractiveSettings, valuesSerialization)
{
    QmlEngine engine;
    engine.load(kTestDataPath + "SimpleSerialization.qml");
    ASSERT_EQ(engine.status(), QmlEngine::Status::loaded);

    const auto values = engine.values();
    const auto expectedValues = engine.rootObject()->property(kValuesProperty).toMap();

    ASSERT_EQ(values, expectedValues);
}

TEST(InteractiveSettings, dependentProperty)
{
    QmlEngine engine;
    engine.load(kTestDataPath + "DependentProperty.qml");
    ASSERT_EQ(engine.status(), QmlEngine::Status::loaded);

    const auto settingsData = engine.serialize();
    const auto expectedData = QJsonObject::fromVariantMap(
        engine.rootObject()->property(kSerializedProperty).toMap());

    ASSERT_EQ(settingsData, expectedData);

    engine.applyValues(QVariantMap{{"master", true}});

    const auto newSettingsData = engine.serialize();
    const auto newExpectedData = QJsonObject::fromVariantMap(
        engine.rootObject()->property(kSerializedProperty).toMap());

    ASSERT_EQ(newSettingsData, newExpectedData);
    ASSERT_NE(settingsData, newSettingsData);
}

TEST(InteractiveSettings, tryValues)
{
    QmlEngine engine;
    engine.load(kTestDataPath + "SimpleSerialization.qml");
    ASSERT_EQ(engine.status(), QmlEngine::Status::loaded);

    const auto originalData = engine.serialize();
    const auto changedData = engine.tryValues(QVariantMap{{"number", 32}});
    const auto restoredData = engine.serialize();

    ASSERT_NE(originalData, changedData);
    ASSERT_EQ(originalData, restoredData);
}

TEST(InteractiveSettings, rangeCheck)
{
    QmlEngine engine;
    engine.load(kTestDataPath + "RangeCheck.qml");
    ASSERT_EQ(engine.status(), QmlEngine::Status::loaded);

    const auto testValues = engine.rootObject()->property(kTestValuesProperty).toMap();
    engine.applyValues(testValues);

    const auto values = engine.values();
    const auto expectedValues = engine.rootObject()->property(kValuesProperty).toMap();

    ASSERT_EQ(values, expectedValues);
}

} // namespace nx::mediaserver::interactive_settings::test
