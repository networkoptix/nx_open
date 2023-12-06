// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "QtCore/qregularexpression.h"
#include "QtGui/qcolor.h"

#include <gtest/gtest.h>
#include <nx/vms/common/html/html.h>

namespace nx::vms::common::html {
namespace test {

TEST(Html, tagged)
{
    ASSERT_EQ(tagged("source", "html"), "<html>source</html>");
    ASSERT_EQ(tagged("source", "div", "style=\"width: 100%\""),
        R"(<div style="width: 100%">source</div>)");
}

TEST(Html, mightBeHtml)
{
    ASSERT_FALSE(mightBeHtml("Tom&Jerry"));
    ASSERT_FALSE(mightBeHtml("Tom & Jerry"));
    ASSERT_FALSE(mightBeHtml("Tom   & Jerry"));
    ASSERT_TRUE(mightBeHtml("Tom&amp;Jerry"));
    ASSERT_TRUE(mightBeHtml("<b>Tom</b>&Jerry"));
    ASSERT_TRUE(mightBeHtml("Tom&<u>Jerry</u>"));
    ASSERT_FALSE(mightBeHtml("1<2"));
    ASSERT_TRUE(mightBeHtml("1&lt;2"));
    ASSERT_FALSE(mightBeHtml("2>1"));
    ASSERT_TRUE(mightBeHtml("2&gt;1"));
    ASSERT_TRUE(mightBeHtml("1 &&nbsp;2"));
    ASSERT_TRUE(mightBeHtml("&quot;"));
    ASSERT_TRUE(mightBeHtml("\"&quot;\""));
}

TEST(Html, highlightMatch)
{
    auto rx = QRegularExpression("fizz", QRegularExpression::CaseInsensitiveOption);
    QColor c("#cccccc");

    ASSERT_EQ(highlightMatch("", rx, c), "");
    ASSERT_EQ(highlightMatch("   ", rx, c), "&nbsp;&nbsp;&nbsp;");
    ASSERT_EQ(highlightMatch("fizz", rx, c), "<font color=\"#cccccc\">fizz</font>");
    ASSERT_EQ(highlightMatch("buzz", rx, c), "buzz");
    ASSERT_EQ(highlightMatch("fizzBUZZ", rx, c), "<font color=\"#cccccc\">fizz</font>BUZZ");
    ASSERT_EQ(highlightMatch("FIZZbuzz", rx, c), "<font color=\"#cccccc\">FIZZ</font>buzz");
    ASSERT_EQ(highlightMatch("fizz BUZZ fizz", rx, c),
        "<font color=\"#cccccc\">fizz</font>&nbsp;BUZZ&nbsp;<font color=\"#cccccc\">fizz</font>");
    ASSERT_EQ(highlightMatch("FizzBuzzFIZZ", rx, c),
        "<font color=\"#cccccc\">Fizz</font>Buzz<font color=\"#cccccc\">FIZZ</font>");

    rx.setPattern("&");
    c = QColor::fromString("#dddddd");
    ASSERT_EQ(highlightMatch("&", rx, c), "<font color=\"#dddddd\">&amp;</font>");
    ASSERT_EQ(highlightMatch("&amp;", rx, c), "<font color=\"#dddddd\">&amp;</font>amp;");
    ASSERT_EQ(highlightMatch("<b>&</b>", rx, c),
        "&lt;b&gt;<font color=\"#dddddd\">&amp;</font>&lt;/b&gt;");
    ASSERT_EQ(highlightMatch("\"&\"", rx, c), "&quot;<font color=\"#dddddd\">&amp;</font>&quot;");
}

} // namespace test
} // namespace nx::vms::common::html
