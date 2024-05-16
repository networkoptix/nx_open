// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <memory>

#include <gtest/gtest.h>

#include <QtGui/QIcon>
#include <QtXml/QDomElement>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/icon.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/skin/svg_icon_colorer.h>
#include <nx/vms/utils/external_resources.h>

namespace {

const QByteArray kSvgData = R"(
    <svg version="1.1" id="Layer_1" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" x="0px" y="0px"
         viewBox="0 0 20 20" enable-background="new 0 0 20 20" xml:space="preserve">
    <!--light10-->
    <path fill="#A5b7C0" d="M13.842,12.161C12.924,13.274,11.552,14,10,14c-2.757,0-5-2.243-5-5V7h8.884
        c0.004-0.005,0.007-0.011,0.012-0.016C14.18,6.676,14.58,6.5,15,6.5h2.846C17.935,6.351,18,6.186,18,6V4c0-0.55-0.45-1-1-1H3
        C2.45,3,2,3.45,2,4v2c0,0.55,0.45,1,1,1v2c0,3.859,3.14,7,7,7c1.552,0,2.983-0.514,4.145-1.373c-0.072-0.156-0.125-0.323-0.14-0.503
        L13.842,12.161z" class="primary"/>
    <!--light4-->
    <circle fill="#E1E7EA" cx="10" cy="11" r="1.7"/>
    <!--dark2-->
    <circle fill="#080707" cx="16.5" cy="17" r="1.7"/>
    <!--blue11-->
    <circle fill="#2592c3" cx="16.5" cy="17" r="1.7"/>
    <!--light1-->
    <circle fill="#FFFfff" cx="16.5" cy="17" r="1.7"/>
    <!--brand_d1-->
    <polygon fill="#2592c3" points="15,8 15.498,13.976 15.5,14 17.5,14 17.502,13.976 18,8 "/>
    <!--unknown-->
    <polygon fill="#123456" points="15,8 15.498,13.976 15.5,14 17.5,14 17.502,13.976 18,8 "/>
    </svg>)";

const QByteArray kSvgDataNormal = R"(
    <svg version="1.1" id="Layer_1" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" x="0px" y="0px"
        viewBox="0 0 20 20" enable-background="new 0 0 20 20" xml:space="preserve">
    <!--light10-->
    <path fill="#b8b8b8" d="M13.842,12.161C12.924,13.274,11.552,14,10,14c-2.757,0-5-2.243-5-5V7h8.884
        c0.004-0.005,0.007-0.011,0.012-0.016C14.18,6.676,14.58,6.5,15,6.5h2.846C17.935,6.351,18,6.186,18,6V4c0-0.55-0.45-1-1-1H3
        C2.45,3,2,3.45,2,4v2c0,0.55,0.45,1,1,1v2c0,3.859,3.14,7,7,7c1.552,0,2.983-0.514,4.145-1.373c-0.072-0.156-0.125-0.323-0.14-0.503
        L13.842,12.161z" class="primary"/>
    <!--light4-->
    <circle fill="#e8e8e8" cx="10" cy="11" r="1.7"/>
    <!--dark2-->
    <circle fill="#080808" cx="16.5" cy="17" r="1.7"/>
    <!--blue11-->
    <circle fill="#f06200" cx="16.5" cy="17" r="1.7"/>
    <!--light1-->
    <circle fill="#FFFfff" cx="16.5" cy="17" r="1.7"/>
    <!--brand_d1-->
    <polygon fill="#f06200" points="15,8 15.498,13.976 15.5,14 17.5,14 17.502,13.976 18,8 "/>
    <!--unknown-->
    <polygon fill="#123456" points="15,8 15.498,13.976 15.5,14 17.5,14 17.502,13.976 18,8 "/>
    </svg>)";

const QByteArray kSvgDataDisabled = R"(
    <svg version="1.1" id="Layer_1" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" x="0px" y="0px"
         viewBox="0 0 20 20" enable-background="new 0 0 20 20" xml:space="preserve">
    <!--light10-->
    <path fill="#686868" d="M13.842,12.161C12.924,13.274,11.552,14,10,14c-2.757,0-5-2.243-5-5V7h8.884
        c0.004-0.005,0.007-0.011,0.012-0.016C14.18,6.676,14.58,6.5,15,6.5h2.846C17.935,6.351,18,6.186,18,6V4c0-0.55-0.45-1-1-1H3
        C2.45,3,2,3.45,2,4v2c0,0.55,0.45,1,1,1v2c0,3.859,3.14,7,7,7c1.552,0,2.983-0.514,4.145-1.373c-0.072-0.156-0.125-0.323-0.14-0.503
        L13.842,12.161z" class="primary"/>
    <!--light4-->
    <circle fill="#808080" cx="10" cy="11" r="1.7"/>
    <!--dark2-->
    <circle fill="#080707" cx="16.5" cy="17" r="1.7"/>
    <!--blue11-->
    <circle fill="#2592c3" cx="16.5" cy="17" r="1.7"/>
    <!--light1-->
    <circle fill="#FFFfff" cx="16.5" cy="17" r="1.7"/>
    <!--brand_d1-->
    <polygon fill="#2592c3" points="15,8 15.498,13.976 15.5,14 17.5,14 17.502,13.976 18,8 "/>
    <!--unknown-->
    <polygon fill="#123456" points="15,8 15.498,13.976 15.5,14 17.5,14 17.502,13.976 18,8 "/>
    </svg>)";

const QByteArray kSvgDataSelected = R"(
    <svg version="1.1" id="Layer_1" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" x="0px" y="0px"
         viewBox="0 0 20 20" enable-background="new 0 0 20 20" xml:space="preserve">
    <!--light10-->
    <path fill="#e8e8e8" d="M13.842,12.161C12.924,13.274,11.552,14,10,14c-2.757,0-5-2.243-5-5V7h8.884
        c0.004-0.005,0.007-0.011,0.012-0.016C14.18,6.676,14.58,6.5,15,6.5h2.846C17.935,6.351,18,6.186,18,6V4c0-0.55-0.45-1-1-1H3
        C2.45,3,2,3.45,2,4v2c0,0.55,0.45,1,1,1v2c0,3.859,3.14,7,7,7c1.552,0,2.983-0.514,4.145-1.373c-0.072-0.156-0.125-0.323-0.14-0.503
        L13.842,12.161z" class="primary"/>
    <!--light4-->
    <circle fill="#ffffff" cx="10" cy="11" r="1.7"/>
    <!--dark2-->
    <circle fill="#080707" cx="16.5" cy="17" r="1.7"/>
    <!--blue11-->
    <circle fill="#2592c3" cx="16.5" cy="17" r="1.7"/>
    <!--light1-->
    <circle fill="#FFFfff" cx="16.5" cy="17" r="1.7"/>
    <!--brand_d1-->
    <polygon fill="#2592c3" points="15,8 15.498,13.976 15.5,14 17.5,14 17.502,13.976 18,8 "/>
    <!--unknown-->
    <polygon fill="#123456" points="15,8 15.498,13.976 15.5,14 17.5,14 17.502,13.976 18,8 "/>
    </svg>)";

const QByteArray kSvgDataActive = R"(
    <svg version="1.1" id="Layer_1" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" x="0px" y="0px"
         viewBox="0 0 20 20" enable-background="new 0 0 20 20" xml:space="preserve">
    <!--light10-->
    <path fill="#ff6c00" d="M13.842,12.161C12.924,13.274,11.552,14,10,14c-2.757,0-5-2.243-5-5V7h8.884
        c0.004-0.005,0.007-0.011,0.012-0.016C14.18,6.676,14.58,6.5,15,6.5h2.846C17.935,6.351,18,6.186,18,6V4c0-0.55-0.45-1-1-1H3
        C2.45,3,2,3.45,2,4v2c0,0.55,0.45,1,1,1v2c0,3.859,3.14,7,7,7c1.552,0,2.983-0.514,4.145-1.373c-0.072-0.156-0.125-0.323-0.14-0.503
        L13.842,12.161z" class="primary"/>
    <!--light4-->
    <circle fill="#f0f0f0" cx="10" cy="11" r="1.7"/>
    <!--dark2-->
    <circle fill="#080707" cx="16.5" cy="17" r="1.7"/>
    <!--blue11-->
    <circle fill="#2592c3" cx="16.5" cy="17" r="1.7"/>
    <!--light1-->
    <circle fill="#FFFfff" cx="16.5" cy="17" r="1.7"/>
    <!--brand_d1-->
    <polygon fill="#2592c3" points="15,8 15.498,13.976 15.5,14 17.5,14 17.502,13.976 18,8 "/>
    <!--unknown-->
    <polygon fill="#123456" points="15,8 15.498,13.976 15.5,14 17.5,14 17.502,13.976 18,8 "/>
    </svg>)";

// Replace light10 with red_l2, light4 with red_l3
const QByteArray kSvgDataError = R"(
    <svg version="1.1" id="Layer_1" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" x="0px" y="0px"
         viewBox="0 0 20 20" enable-background="new 0 0 20 20" xml:space="preserve">
    <!--light10-->
    <path fill="#f02c2c" d="M13.842,12.161C12.924,13.274,11.552,14,10,14c-2.757,0-5-2.243-5-5V7h8.884
        c0.004-0.005,0.007-0.011,0.012-0.016C14.18,6.676,14.58,6.5,15,6.5h2.846C17.935,6.351,18,6.186,18,6V4c0-0.55-0.45-1-1-1H3
        C2.45,3,2,3.45,2,4v2c0,0.55,0.45,1,1,1v2c0,3.859,3.14,7,7,7c1.552,0,2.983-0.514,4.145-1.373c-0.072-0.156-0.125-0.323-0.14-0.503
        L13.842,12.161z" class="primary"/>
    <!--light4-->
    <circle fill="#ef5350" cx="10" cy="11" r="1.7"/>
    <!--dark2-->
    <circle fill="#080707" cx="16.5" cy="17" r="1.7"/>
    <!--blue11-->
    <circle fill="#2592c3" cx="16.5" cy="17" r="1.7"/>
    <!--light1-->
    <circle fill="#FFFfff" cx="16.5" cy="17" r="1.7"/>
    <!--brand_d1-->
    <polygon fill="#2592c3" points="15,8 15.498,13.976 15.5,14 17.5,14 17.502,13.976 18,8 "/>
    <!--unknown-->
    <polygon fill="#123456" points="15,8 15.498,13.976 15.5,14 17.5,14 17.502,13.976 18,8 "/>
    </svg>)";

} // namespace

namespace nx::vms::client::core {

class SvgIconColorerTest: public testing::Test
{
public:
    std::unique_ptr<core::SvgIconColorer> m_colorer;

protected:
    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp() override
    {
        nx::vms::utils::registerExternalResource("client_core_external.dat");

        static const QMap<QIcon::Mode, SvgIconColorer::ThemeColorsRemapData> themeSubstitutions = {
            {QIcon::Normal, { .primary = "light10"}},
            {QIcon::Disabled, { .primary = "dark14"}},
            {QIcon::Selected, { .primary = "light4"}},
            {QIcon::Active, { .primary = "brand_core"}},
            {QnIcon::Error, { .primary = "red1"}},
        };

        m_colorer.reset(new SvgIconColorer(kSvgData,
            "mock_svg",
            SvgIconColorer::kTreeIconSubstitutions,
            themeSubstitutions));
        m_colorTheme.reset(new ColorTheme(
            ":/skin/basic_colors.json", ":/unit_tests/svg_icon_colorer_ut/test_skin_colors.json"));
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown() override
    {
        m_colorTheme.reset();

        ASSERT_TRUE(nx::vms::utils::unregisterExternalResource("client_core_external.dat"));
    }

private:
    std::unique_ptr<core::ColorTheme> m_colorTheme;
};

TEST_F(SvgIconColorerTest, checkNormalIcon)
{
    QDomDocument actual;
    ASSERT_TRUE(actual.setContent(m_colorer->makeIcon(QnIcon::Normal)));
    QDomDocument expected;
    ASSERT_TRUE(expected.setContent(kSvgDataNormal));
    ASSERT_EQ(actual.toByteArray(), expected.toByteArray());
}

TEST_F(SvgIconColorerTest, checkDisabledIcon)
{
    QDomDocument actual;
    ASSERT_TRUE(actual.setContent(m_colorer->makeIcon(QnIcon::Disabled)));
    QDomDocument expected;
    ASSERT_TRUE(expected.setContent(kSvgDataDisabled));
    ASSERT_EQ(actual.toByteArray(), expected.toByteArray());
}

TEST_F(SvgIconColorerTest, checkSelectedIcon)
{
    QDomDocument actual;
    ASSERT_TRUE(actual.setContent(m_colorer->makeIcon(QnIcon::Selected)));
    QDomDocument expected;
    ASSERT_TRUE(expected.setContent(kSvgDataSelected));
    ASSERT_EQ(actual.toByteArray(), expected.toByteArray());
}

TEST_F(SvgIconColorerTest, checkActiveIcon)
{
    QDomDocument actual;
    ASSERT_TRUE(actual.setContent(m_colorer->makeIcon(QnIcon::Active)));
    QDomDocument expected;
    ASSERT_TRUE(expected.setContent(kSvgDataActive));
    ASSERT_EQ(actual.toByteArray(), expected.toByteArray());
}

TEST_F(SvgIconColorerTest, checkErrorIcon)
{
    QDomDocument actual;
    ASSERT_TRUE(actual.setContent(m_colorer->makeIcon(QnIcon::Error)));
    QDomDocument expected;
    ASSERT_TRUE(expected.setContent(kSvgDataError));
    ASSERT_EQ(actual.toByteArray(), expected.toByteArray());
}

} // namespace nx::vms::client::desktop
