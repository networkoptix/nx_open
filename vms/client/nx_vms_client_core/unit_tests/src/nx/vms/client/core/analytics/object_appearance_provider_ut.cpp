// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <memory>
#include <optional>
#include <string>

#include <gtest/gtest.h>

#include <analytics/common/object_metadata.h>
#include <nx/vms/client/core/analytics/object_appearance_provider.h>
#include <nx/vms/client/core/application_context.h>

namespace nx::vms::client::core::analytics::test {

using namespace nx::common::metadata;

const std::string kPersonTypeId = "nx.base.Person";

class ObjectAppearanceProviderTest: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        appContext = std::make_unique<ApplicationContext>(ApplicationContext::Mode::unitTests,
            Qn::SerializationFormat::json,
            nx::vms::api::PeerType::mobileClient);

        object.trackId = nx::Uuid::createUuid();
        object.typeId = kPersonTypeId;
    }

    virtual void TearDown() override
    {
        provider.reset();
        appContext.reset();
    }

    void givenProviderWithoutSystemContext()
    {
        provider = std::make_unique<ObjectAppearanceProvider>(/*systemContext*/ nullptr);
    }

    void givenObjectAttribute(const QString& name, const QString& value)
    {
        object.attributes.push_back({name, value});
    }

    void whenObjectTypeNameIsRequested()
    {
        objectTypeName = provider->objectTypeDictionary()->idToName(object.typeId);
    }

    void thenTitleIs(const QString& title) const { EXPECT_EQ(title, provider->title(object)); }

    void thenColorIs(const QColor& color) const { EXPECT_EQ(color, provider->color(object)); }

    void thenObjectTypeNameIsUnknown() const { EXPECT_EQ(std::nullopt, objectTypeName); }

protected:
    std::unique_ptr<ApplicationContext> appContext;
    std::unique_ptr<ObjectAppearanceProvider> provider;
    ObjectMetadata object;

    std::optional<std::string> objectTypeName;
};

// Without a System, e.g. before the video resource is known.

TEST_F(ObjectAppearanceProviderTest, titleIsEmptyWithoutSystemContext)
{
    givenProviderWithoutSystemContext();

    thenTitleIs("");
}

TEST_F(ObjectAppearanceProviderTest, objectTypesAreUnknownWithoutSystemContext)
{
    givenProviderWithoutSystemContext();

    whenObjectTypeNameIsRequested();

    thenObjectTypeNameIsUnknown();
}

TEST_F(ObjectAppearanceProviderTest, colorIsAvailableWithoutSystemContext)
{
    givenProviderWithoutSystemContext();
    givenObjectAttribute("nx.sys.color", "#123456");

    thenColorIs(QColor("#123456"));
}

} // namespace nx::vms::client::core::analytics::test
