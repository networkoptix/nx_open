// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/branding.h>
#include <nx/reflect/json.h>
#include <nx/utils/i18n/scoped_locale.h>
#include <nx/utils/i18n/translation_manager.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/translatable_string.h>

namespace nx::utils::test {

namespace {

static const QString kLocaleEn("en_US");
static const QString kValueEn("Hello!");

static const QString kLocaleEs("es_ES");
static const QString kValueEs("Hola!");

static const QString kLocaleFr("fr_FR");
static const QString kValueFr("Bonjour!");


struct DataWithTranslatableString
{
    TranslatableString field;
};
NX_REFLECTION_INSTRUMENT(DataWithTranslatableString, (field))

} // namespace

TEST(TranslatableString, createWithProvider)
{
    TranslatableString s([] { return kValueEn; });
    ASSERT_EQ(s, kValueEn);
}

TEST(TranslatableString, createWithExplicitValues)
{
    TranslatableString s({{kLocaleEn, kValueEn}, {kLocaleEs, kValueEs}});
    ASSERT_EQ(s.value(kLocaleEn), kValueEn);
    ASSERT_EQ(s.value(kLocaleEs), kValueEs);
}

TEST(TranslatableString, checkExplicitDefaultValue)
{
    TranslatableString s(
        {{TranslatableString::kDefaultValueKey, kValueEn}, {kLocaleEs, kValueEs}});
    ASSERT_EQ(s.value(kLocaleFr), kValueEn);
}

TEST(TranslatableString, checkExplicitDefaultLocale)
{
    static const QString currentLocale = nx::branding::defaultLocale();
    TranslatableString s({{currentLocale, kValueEn}, {kLocaleEs, kValueEs}});
    ASSERT_EQ(s.value(kLocaleFr), kValueEn);
}

TEST(TranslatableString, deserializeFromStringReflect)
{
    const auto [d, result] = nx::reflect::json::deserialize<DataWithTranslatableString>(
        R"json({ "field": "Hello" })json");
    EXPECT_TRUE(result.success);
    ASSERT_EQ(d.field, "Hello");
}

TEST(TranslatableString, deserializeFromMapReflect)
{
    const auto [d, result] = nx::reflect::json::deserialize<DataWithTranslatableString>(
        R"json({ "field": { "_": "Hello", "es_ES": "Hola" } })json");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(d.field.value("es_ES"), "Hola");
    EXPECT_EQ(d.field.value("pt_PT"), "Hello");
}

TEST(TranslatableString, serializeToStringReflect)
{
    DataWithTranslatableString d{.field = TranslatableString("Hello")};
    EXPECT_EQ(nx::reflect::json::serialize(d), R"json({"field":"Hello"})json");
}

TEST(TranslatableString, checkCurrentLocaleWithExplicitValues)
{
    static const QString currentLocale = nx::branding::defaultLocale();
    TranslatableString s({
        {TranslatableString::kDefaultValueKey, kValueFr},
        {currentLocale, kValueEn},
        {kLocaleEs, kValueEs}
    });
    NX_ASSERT(currentLocale != kLocaleFr && currentLocale != kLocaleEs,
        "Test cannot be executed with this default locale");

    EXPECT_EQ(s, kValueEn);

    i18n::TranslationManager translationManager;
    translationManager.startLoadingTranslations();
    {
        auto scopedTranslation = translationManager.installScopedLocale(kLocaleEs);
        EXPECT_EQ(s, kValueEs);
    }

    {
        auto scopedTranslation = translationManager.installScopedLocale(kLocaleFr);
        EXPECT_EQ(s, kValueFr);
    }
}

} // namespace nx::utils::test
