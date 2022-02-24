// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/rules/actions/send_email_action.h>

namespace nx::vms::rules::test {

namespace {

template<class T>
void testManifestValidity()
{
    const auto manifest = T::manifest();
    const auto meta = T::staticMetaObject;

    EXPECT_FALSE(manifest.id.isEmpty());
    EXPECT_FALSE(manifest.displayName.isEmpty());

    // Check all manifest fields correspond to properties with the same name.
    for (const auto& field: manifest.fields)
    {
        EXPECT_FALSE(field.id.isEmpty());
        EXPECT_FALSE(field.displayName.isEmpty());
        EXPECT_FALSE(field.fieldName.isEmpty());

        EXPECT_GE(meta.indexOfProperty(field.fieldName.toUtf8().data()), 0);
    }
}

} //namespace

TEST(ManifestValidity, BuiltinActions)
{
    testManifestValidity<SendEmailAction>();
}

} // namespace nx::vms::rules::test
