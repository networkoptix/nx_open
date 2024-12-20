// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <variant>

#include <gtest/gtest.h>

#include <nx/fusion/serialization/json_functions.h>
#include <nx/reflect/json/deserializer.h>
#include <nx/utils/json/qjson.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>

namespace nx::vms::api::analytics::test {

TEST(Analytics, Deserialization)
{
    const std::string manifestStr = /*suppress newline*/ 1 + (const char*)
    R"json(
    {
        "supportedTypes": [],
        "typeLibrary":
        {
            "objectTypes":
            [
                {
                    "id": "myCompany.car",
                    "attributes":
                    [
                        {
                            "type": "enum",
                            "name": "make",
                            "items":
                            [
                                "China noname",
                                {
                                    "value": "Nissan",
                                    "dependentAttributes":
                                    [
                                        {
                                            "type": "enum",
                                            "name": "model",
                                            "items": [ "model1", "model2", "model3" ]
                                        }
                                    ]
                                }
                            ]
                        }
                    ]
                }
            ],
            "enumTypes":
            [
            ]
        }
    })json";

    {
        // To make sure the file structure is valid, check that the string deserializes QJsonObject first,
        // and check that the ItemObject contents in it is valid

        auto [fullData, result] =
            nx::reflect::json::deserialize<QJsonObject>(manifestStr);

        ASSERT_TRUE(!fullData.empty());

        auto typeLibrary = fullData["typeLibrary"].toObject();
        auto objectTypes = typeLibrary["objectTypes"].toArray();
        auto firstObject = objectTypes[0].toObject();
        auto attributes = firstObject["attributes"].toArray();
        auto firstAttribute = attributes[0].toObject();
        auto items = firstAttribute["items"].toArray();
        ASSERT_EQ(items[0], "China noname");
        auto itemObject = items[1].toObject();
        ASSERT_EQ(itemObject["value"], "Nissan");
        auto dependentAttributes = itemObject["dependentAttributes"].toArray();
        auto firstDependentAttribute = dependentAttributes[0].toObject();
        ASSERT_EQ(firstDependentAttribute["type"], "enum");
        ASSERT_EQ(firstDependentAttribute["name"], "model");
        QJsonArray dependentAttributeItems;
        dependentAttributeItems.push_back("model1");
        dependentAttributeItems.push_back("model2");
        dependentAttributeItems.push_back("model3");
        ASSERT_EQ(firstDependentAttribute["items"].toArray(), dependentAttributeItems);
    }

    {
        const std::string itemObjectString = /*suppress newline*/ 1 + (const char*)
        R"json(
        {
            "value": "Nissan",
            "dependentAttributes":
            [
                {
                    "type": "enum",
                    "name": "model",
                    "items": [ "model1", "model2", "model3" ]
                }
            ]
        })json";

        const auto checkItemObject = [](const ItemObject& object) {
            auto dependentAttributes = object.dependentAttributes;

            ASSERT_EQ(object.value, "Nissan");
            ASSERT_EQ(dependentAttributes.size(), 1);

            auto& dependentAttribute = dependentAttributes[0];
            ASSERT_EQ(dependentAttribute.type, AttributeType::enumeration);
            ASSERT_EQ(dependentAttribute.name, "model");
            ASSERT_NE(dependentAttribute.items, std::nullopt);
            std::vector<Item> dependentAttributeItems;
            dependentAttributeItems.push_back("model1");
            dependentAttributeItems.push_back("model2");
            dependentAttributeItems.push_back("model3");
            ASSERT_EQ(*dependentAttribute.items, dependentAttributeItems);
        };

        {
            auto [object, result] =
                nx::reflect::json::deserialize<ItemObject>(itemObjectString);

            ASSERT_TRUE(result.success);

            checkItemObject(object);
        }
        {
            auto object = QJson::deserialized<ItemObject>(QByteArray::fromStdString(
                itemObjectString));

            checkItemObject(object);
        }

    }

    {
        const std::string itemObjectString = /*suppress newline*/ 1 + (const char*)
        R"json(
        {
            "value": "Nissan",
            "dependentAttributes":
            [
                {
                    "type": "enum",
                    "name": "model",
                    "items": [ "model1", "model2", "model3" ]
                }
            ]
        })json";


        const auto checkVariant = [](const std::variant<ItemObject,QString>& objectVariant) {
            ASSERT_TRUE(std::holds_alternative<ItemObject>(objectVariant));

            const ItemObject& object = std::get<ItemObject>(objectVariant);

            auto dependentAttributes = object.dependentAttributes;

            ASSERT_EQ(object.value, "Nissan");
            ASSERT_EQ(dependentAttributes.size(), 1);

            auto& dependentAttribute = dependentAttributes[0];
            ASSERT_EQ(dependentAttribute.type, AttributeType::enumeration);
            ASSERT_EQ(dependentAttribute.name, "model");
            ASSERT_NE(dependentAttribute.items, std::nullopt);
            std::vector<Item> dependentAttributeItems;
            dependentAttributeItems.push_back("model1");
            dependentAttributeItems.push_back("model2");
            dependentAttributeItems.push_back("model3");
            ASSERT_EQ(*dependentAttribute.items, dependentAttributeItems);
        };

        {
            auto [objectVariant, result] =
                nx::reflect::json::deserialize<Item>(itemObjectString);

            ASSERT_TRUE(result.success);

            checkVariant(objectVariant);
        }
        {
            auto objectVariant = QJson::deserialized<Item>(
                QByteArray::fromStdString(itemObjectString));

            checkVariant(objectVariant);
        }
    }

    {
        const std::string arrayString = /*suppress newline*/ 1 + (const char*)
        R"json(
        [
            "China noname",
            {
                "value": "Nissan",
                "dependentAttributes":
                [
                    {
                        "type": "enum",
                        "name": "model",
                        "items": [ "model1", "model2", "model3" ]
                    }
                ]
            }
        ])json";

        const auto checkVectorOfItems = [](const std::vector<Item>& items) {
            ASSERT_EQ(items.size(), 2);
            ASSERT_TRUE(std::holds_alternative<QString>(items[0]));
            ASSERT_EQ(std::get<QString>(items[0]), "China noname");
            ASSERT_TRUE(std::holds_alternative<ItemObject>(items[1]));

            const ItemObject& object = std::get<ItemObject>(items[1]);
            auto dependentAttributes = object.dependentAttributes;

            ASSERT_EQ(object.value, "Nissan");
            ASSERT_EQ(dependentAttributes.size(), 1);

            auto& dependentAttribute = dependentAttributes[0];
            ASSERT_EQ(dependentAttribute.type, AttributeType::enumeration);
            ASSERT_EQ(dependentAttribute.name, "model");
            ASSERT_NE(dependentAttribute.items, std::nullopt);
            std::vector<Item> dependentAttributeItems;
            dependentAttributeItems.push_back("model1");
            dependentAttributeItems.push_back("model2");
            dependentAttributeItems.push_back("model3");
            ASSERT_EQ(*dependentAttribute.items, dependentAttributeItems);
        };

        {
            auto [items, result] =
                nx::reflect::json::deserialize<std::vector<Item>>(arrayString);

            ASSERT_TRUE(result.success);

            checkVectorOfItems(items);
        }
        {
            auto items = QJson::deserialized<std::vector<Item>>(
                QByteArray::fromStdString(arrayString));

            checkVectorOfItems(items);
        }
    }

    {
        const auto checkManifest = [](const DeviceAgentManifest& manifest) {
            auto& typeLibrary = manifest.typeLibrary;
            auto& objectTypes = typeLibrary.objectTypes;
            auto& enumTypes = typeLibrary.enumTypes;

            ASSERT_EQ(objectTypes.size(), 1);
            ASSERT_EQ(enumTypes.size(), 0);

            auto& objectType = objectTypes[0];
            auto& attributes = objectType.attributes;
            ASSERT_EQ(objectType.id, "myCompany.car");
            ASSERT_EQ(attributes.size(), 1);

            auto& attribute = attributes[0];

            ASSERT_EQ(attribute.type, AttributeType::enumeration);
            ASSERT_EQ(attribute.name, "make");

            ASSERT_NE(attribute.items, std::nullopt);

            auto items = *attribute.items;
            ASSERT_EQ(items.size(), 2);
            ASSERT_TRUE(std::holds_alternative<QString>(items[0]));
            ASSERT_EQ(std::get<QString>(items[0]), "China noname");
            ASSERT_TRUE(std::holds_alternative<ItemObject>(items[1]));

            ItemObject& object = std::get<ItemObject>(items[1]);
            auto dependentAttributes = object.dependentAttributes;

            ASSERT_EQ(object.value, "Nissan");
            ASSERT_EQ(dependentAttributes.size(), 1);

            auto& dependentAttribute = dependentAttributes[0];
            ASSERT_EQ(dependentAttribute.type, AttributeType::enumeration);
            ASSERT_EQ(dependentAttribute.name, "model");
            ASSERT_NE(dependentAttribute.items, std::nullopt);
            std::vector<Item> dependentAttributeItems;
            dependentAttributeItems.push_back("model1");
            dependentAttributeItems.push_back("model2");
            dependentAttributeItems.push_back("model3");
            ASSERT_EQ(*dependentAttribute.items, dependentAttributeItems);
        };

        {
            auto [manifest, result] =
                nx::reflect::json::deserialize<DeviceAgentManifest>(manifestStr);

            ASSERT_TRUE(result.success);

            checkManifest(manifest);
        }
        {
            auto manifest = QJson::deserialized<DeviceAgentManifest>(
                QByteArray::fromStdString(manifestStr));

            checkManifest(manifest);
        }
    }
}

} // namespace nx::vms::api::analytics::test
