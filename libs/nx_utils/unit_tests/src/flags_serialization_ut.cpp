// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/reflect/json.h>
#include <nx/utils/json/flags.h>
#include <nx/utils/nx_utils_ini.h>

NX_REFLECTION_ENUM_CLASS(Permission, none = 0, read = 0x1, write = 0x2, execute = 0x4)
Q_DECLARE_FLAGS(Permissions, Permission)

TEST(FlagsSerialization, Flags)
{
    auto flags = Permissions(Permission::read) | Permission::write;
    EXPECT_EQ("read|write", toString(flags));

    fromString("execute|read", &flags);
    EXPECT_EQ(flags, Permissions(Permission::read) | Permission::execute);

    fromString("write", &flags);
    EXPECT_EQ(flags, Permissions(Permission::write));
}

TEST(FlagsSerialization, InvalidFlags)
{
    Permissions flags;
    EXPECT_FALSE(fromString("execute|append", &flags));
    EXPECT_EQ(flags, Permissions(Permission::execute));

    nx::kit::IniConfig::Tweaks iniTweaks;
    iniTweaks.set(&nx::utils::ini().assertCrash, true);

    EXPECT_DEATH(toString(Permissions(static_cast<Permission>(0x8))), "");
}

TEST(FlagsSerialization, SpacesAreAllowedInDeserialization)
{
    Permissions flags;
    EXPECT_TRUE(fromString("read | write | execute", &flags));
    EXPECT_EQ(flags, Permissions(Permission::read) | Permission::write | Permission::execute);
}

NX_REFLECTION_ENUM_CLASS(BoundaryFlag,
    smallest = 0x1, //< The smallest possible flag.
    biggest = 0x4000000 //< The biggest possible flag.
)
Q_DECLARE_FLAGS(BoundaryFlags, BoundaryFlag)

TEST(FlagsSerialization, BoundaryFlags)
{
    auto flags = BoundaryFlags(BoundaryFlag::smallest) | BoundaryFlag::biggest;
    EXPECT_EQ("smallest|biggest", toString(flags));

    flags = {};
    fromString("smallest|biggest", &flags);
    EXPECT_EQ(flags, BoundaryFlags(BoundaryFlag::smallest) | BoundaryFlag::biggest);
}

TEST(FlagsSerialization, EmptyFlagsSerializedToZeroConstantName)
{
    EXPECT_EQ("none", toString(Permissions()));
    EXPECT_EQ("", toString(BoundaryFlags()));
}

struct StructWithFlags
{
    Permissions permissions;
};
NX_REFLECTION_INSTRUMENT(StructWithFlags, (permissions))

TEST(FlagsSerialization, structWithFlagsSerialize)
{
    const StructWithFlags flags{Permissions(Permission::read) | Permission::write};

    const auto serialized = nx::reflect::json::serialize(flags);
    const auto expected = R"json({"permissions":"read|write"})json";
    EXPECT_EQ(serialized, expected);
}

TEST(FlagsSerialization, structWithFlagsDeserialize)
{
    const auto serialized = R"json({"permissions":"read|execute"})json";
    const auto expectedPermissions = Permissions(Permission::read) | Permission::execute;

    StructWithFlags flags;
    ASSERT_TRUE(nx::reflect::json::deserialize(serialized, &flags));
    EXPECT_EQ(flags.permissions, expectedPermissions);
}
