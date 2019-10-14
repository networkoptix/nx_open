#include <gtest/gtest.h>

#include <memory>

#include <ui/style/svg_icon_colorer.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>

namespace {

const QByteArray svg_data = R"(
    <?xml version="1.0" encoding="utf-8"?>
    <svg version="1.1" id="Layer_1" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" x="0px" y="0px"
         viewBox="0 0 20 20" enable-background="new 0 0 20 20" xml:space="preserve">
    <path fill="#A5b7C0" d="M13.842,12.161C12.924,13.274,11.552,14,10,14c-2.757,0-5-2.243-5-5V7h8.884                                   <!--light10-->
        c0.004-0.005,0.007-0.011,0.012-0.016C14.18,6.676,14.58,6.5,15,6.5h2.846C17.935,6.351,18,6.186,18,6V4c0-0.55-0.45-1-1-1H3
        C2.45,3,2,3.45,2,4v2c0,0.55,0.45,1,1,1v2c0,3.859,3.14,7,7,7c1.552,0,2.983-0.514,4.145-1.373c-0.072-0.156-0.125-0.323-0.14-0.503
        L13.842,12.161z"/>
    <circle fill="#E1E7EA" cx="10" cy="11" r="1.7"/>                                                                                    <!--light4-->
    <circle fill="#080707" cx="16.5" cy="17" r="1.7"/>                                                                                  <!--dark2-->
    <circle fill="#2592c3" cx="16.5" cy="17" r="1.7"/>                                                                                  <!--blue11-->
    <circle fill="#FFFfff" cx="16.5" cy="17" r="1.7"/>                                                                                  <!--light1-->
    <polygon fill="#2592c3" points="15,8 15.498,13.976 15.5,14 17.5,14 17.502,13.976 18,8 "/>                                           <!--brand_d1-->
    <polygon fill="#123456" points="15,8 15.498,13.976 15.5,14 17.5,14 17.502,13.976 18,8 "/>                                           <!--unknown-->
    </svg>)";

const QByteArray svg_data_normal = R"(
    <?xml version="1.0" encoding="utf-8"?>
    <svg version="1.1" id="Layer_1" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" x="0px" y="0px"
         viewBox="0 0 20 20" enable-background="new 0 0 20 20" xml:space="preserve">
    <path fill="#b8b8b8" d="M13.842,12.161C12.924,13.274,11.552,14,10,14c-2.757,0-5-2.243-5-5V7h8.884                                   <!--light10-->
        c0.004-0.005,0.007-0.011,0.012-0.016C14.18,6.676,14.58,6.5,15,6.5h2.846C17.935,6.351,18,6.186,18,6V4c0-0.55-0.45-1-1-1H3
        C2.45,3,2,3.45,2,4v2c0,0.55,0.45,1,1,1v2c0,3.859,3.14,7,7,7c1.552,0,2.983-0.514,4.145-1.373c-0.072-0.156-0.125-0.323-0.14-0.503
        L13.842,12.161z"/>
    <circle fill="#e8e8e8" cx="10" cy="11" r="1.7"/>                                                                                    <!--light4-->
    <circle fill="#080808" cx="16.5" cy="17" r="1.7"/>                                                                                  <!--dark2-->
    <circle fill="#f06200" cx="16.5" cy="17" r="1.7"/>                                                                                  <!--blue11-->
    <circle fill="#ffffff" cx="16.5" cy="17" r="1.7"/>                                                                                  <!--light1-->
    <polygon fill="#f06200" points="15,8 15.498,13.976 15.5,14 17.5,14 17.502,13.976 18,8 "/>                                           <!--brand_d1-->
    <polygon fill="#123456" points="15,8 15.498,13.976 15.5,14 17.5,14 17.502,13.976 18,8 "/>                                           <!--unknown-->
    </svg>)";

const QByteArray svg_data_disabled = R"(
    <?xml version="1.0" encoding="utf-8"?>
    <svg version="1.1" id="Layer_1" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" x="0px" y="0px"
         viewBox="0 0 20 20" enable-background="new 0 0 20 20" xml:space="preserve">
    <path fill="#686868" d="M13.842,12.161C12.924,13.274,11.552,14,10,14c-2.757,0-5-2.243-5-5V7h8.884                                   <!--light10-->
        c0.004-0.005,0.007-0.011,0.012-0.016C14.18,6.676,14.58,6.5,15,6.5h2.846C17.935,6.351,18,6.186,18,6V4c0-0.55-0.45-1-1-1H3
        C2.45,3,2,3.45,2,4v2c0,0.55,0.45,1,1,1v2c0,3.859,3.14,7,7,7c1.552,0,2.983-0.514,4.145-1.373c-0.072-0.156-0.125-0.323-0.14-0.503
        L13.842,12.161z"/>
    <circle fill="#808080" cx="10" cy="11" r="1.7"/>                                                                                    <!--light4-->
    <circle fill="#080707" cx="16.5" cy="17" r="1.7"/>                                                                                  <!--dark2-->
    <circle fill="#2592c3" cx="16.5" cy="17" r="1.7"/>                                                                                  <!--blue11-->
    <circle fill="#FFFfff" cx="16.5" cy="17" r="1.7"/>                                                                                  <!--light1-->
    <polygon fill="#2592c3" points="15,8 15.498,13.976 15.5,14 17.5,14 17.502,13.976 18,8 "/>                                           <!--brand_d1-->
    <polygon fill="#123456" points="15,8 15.498,13.976 15.5,14 17.5,14 17.502,13.976 18,8 "/>                                           <!--unknown-->
    </svg>)";

const QByteArray svg_data_selected = R"(
    <?xml version="1.0" encoding="utf-8"?>
    <svg version="1.1" id="Layer_1" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" x="0px" y="0px"
         viewBox="0 0 20 20" enable-background="new 0 0 20 20" xml:space="preserve">
    <path fill="#e8e8e8" d="M13.842,12.161C12.924,13.274,11.552,14,10,14c-2.757,0-5-2.243-5-5V7h8.884                                   <!--light10-->
        c0.004-0.005,0.007-0.011,0.012-0.016C14.18,6.676,14.58,6.5,15,6.5h2.846C17.935,6.351,18,6.186,18,6V4c0-0.55-0.45-1-1-1H3
        C2.45,3,2,3.45,2,4v2c0,0.55,0.45,1,1,1v2c0,3.859,3.14,7,7,7c1.552,0,2.983-0.514,4.145-1.373c-0.072-0.156-0.125-0.323-0.14-0.503
        L13.842,12.161z"/>
    <circle fill="#ffffff" cx="10" cy="11" r="1.7"/>                                                                                    <!--light4-->
    <circle fill="#080707" cx="16.5" cy="17" r="1.7"/>                                                                                  <!--dark2-->
    <circle fill="#2592c3" cx="16.5" cy="17" r="1.7"/>                                                                                  <!--blue11-->
    <circle fill="#FFFfff" cx="16.5" cy="17" r="1.7"/>                                                                                  <!--light1-->
    <polygon fill="#2592c3" points="15,8 15.498,13.976 15.5,14 17.5,14 17.502,13.976 18,8 "/>                                           <!--brand_d1-->
    <polygon fill="#123456" points="15,8 15.498,13.976 15.5,14 17.5,14 17.502,13.976 18,8 "/>                                           <!--unknown-->
    </svg>)";

const QByteArray svg_data_active = R"(
    <?xml version="1.0" encoding="utf-8"?>
    <svg version="1.1" id="Layer_1" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" x="0px" y="0px"
         viewBox="0 0 20 20" enable-background="new 0 0 20 20" xml:space="preserve">
    <path fill="#ff6c00" d="M13.842,12.161C12.924,13.274,11.552,14,10,14c-2.757,0-5-2.243-5-5V7h8.884                                   <!--light10-->
        c0.004-0.005,0.007-0.011,0.012-0.016C14.18,6.676,14.58,6.5,15,6.5h2.846C17.935,6.351,18,6.186,18,6V4c0-0.55-0.45-1-1-1H3
        C2.45,3,2,3.45,2,4v2c0,0.55,0.45,1,1,1v2c0,3.859,3.14,7,7,7c1.552,0,2.983-0.514,4.145-1.373c-0.072-0.156-0.125-0.323-0.14-0.503
        L13.842,12.161z"/>
    <circle fill="#ff932a" cx="10" cy="11" r="1.7"/>                                                                                    <!--light4-->
    <circle fill="#080707" cx="16.5" cy="17" r="1.7"/>                                                                                  <!--dark2-->
    <circle fill="#2592c3" cx="16.5" cy="17" r="1.7"/>                                                                                  <!--blue11-->
    <circle fill="#FFFfff" cx="16.5" cy="17" r="1.7"/>                                                                                  <!--light1-->
    <polygon fill="#2592c3" points="15,8 15.498,13.976 15.5,14 17.5,14 17.502,13.976 18,8 "/>                                           <!--brand_d1-->
    <polygon fill="#123456" points="15,8 15.498,13.976 15.5,14 17.5,14 17.502,13.976 18,8 "/>                                           <!--unknown-->
    </svg>)";

const QByteArray svg_data_error = R"(
    <?xml version="1.0" encoding="utf-8"?>
    <svg version="1.1" id="Layer_1" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" x="0px" y="0px"
         viewBox="0 0 20 20" enable-background="new 0 0 20 20" xml:space="preserve">
    <path fill="#f02c2c" d="M13.842,12.161C12.924,13.274,11.552,14,10,14c-2.757,0-5-2.243-5-5V7h8.884                                   <!--light10-->
        c0.004-0.005,0.007-0.011,0.012-0.016C14.18,6.676,14.58,6.5,15,6.5h2.846C17.935,6.351,18,6.186,18,6V4c0-0.55-0.45-1-1-1H3
        C2.45,3,2,3.45,2,4v2c0,0.55,0.45,1,1,1v2c0,3.859,3.14,7,7,7c1.552,0,2.983-0.514,4.145-1.373c-0.072-0.156-0.125-0.323-0.14-0.503
        L13.842,12.161z"/>
    <circle fill="#000000" cx="10" cy="11" r="1.7"/>                                                                                    <!--light4-->
    <circle fill="#080707" cx="16.5" cy="17" r="1.7"/>                                                                                  <!--dark2-->
    <circle fill="#2592c3" cx="16.5" cy="17" r="1.7"/>                                                                                  <!--blue11-->
    <circle fill="#FFFfff" cx="16.5" cy="17" r="1.7"/>                                                                                  <!--light1-->
    <polygon fill="#2592c3" points="15,8 15.498,13.976 15.5,14 17.5,14 17.502,13.976 18,8 "/>                                           <!--brand_d1-->
    <polygon fill="#123456" points="15,8 15.498,13.976 15.5,14 17.5,14 17.502,13.976 18,8 "/>                                           <!--unknown-->
    </svg>)";

} // namespace


namespace nx::vms::client::desktop {

class SvgIconColorerTest: public testing::Test
{
public:
    std::unique_ptr<SvgIconColorer> m_colorer;

protected:
    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp() override
    {
        m_colorer.reset(new SvgIconColorer(svg_data, "mock_svg"));
        m_colorTheme.reset(new ColorTheme(
            ":/svg_icon_colorer_ut/customization_common.json",
            ":/svg_icon_colorer_ut/skin.json"));
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown() override
    {
        m_colorTheme.reset();
    }

private:
    std::unique_ptr<ColorTheme> m_colorTheme;
};

TEST_F(SvgIconColorerTest, checkNormalIcon)
{
    ASSERT_EQ(m_colorer->makeNormalIcon(), svg_data_normal);
}

TEST_F(SvgIconColorerTest, checkDisabledIcon)
{
    ASSERT_EQ(m_colorer->makeDisabledIcon(), svg_data_disabled);
}

TEST_F(SvgIconColorerTest, checkSelectedIcon)
{
    ASSERT_EQ(m_colorer->makeSelectedIcon(), svg_data_selected);
}

TEST_F(SvgIconColorerTest, checkActiveIcon)
{
    ASSERT_EQ(m_colorer->makeActiveIcon(), svg_data_active);
}

TEST_F(SvgIconColorerTest, checkErrorIcon)
{
    ASSERT_EQ(m_colorer->makeErrorIcon(), svg_data_error);
}

} // namespace nx::vms::client::desktop
