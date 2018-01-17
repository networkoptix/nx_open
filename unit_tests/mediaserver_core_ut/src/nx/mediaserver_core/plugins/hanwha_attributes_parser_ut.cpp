#if defined(ENABLE_HANWHA)

#include <gtest/gtest.h>

#include <plugins/resource/hanwha/hanwha_attributes.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {
namespace test {

struct HanwhaAttributesParser: ::testing::Test
{
    HanwhaAttributes attributes;

    QString attrValue(const QString& path)
    {
        auto result = attributes.attribute<QString>(path);
        return result ? *result : QString();
    }

    void shortTest(const QString& data)
    {
        attributes = HanwhaAttributes(data, nx::network::http::StatusCode::ok);
        ASSERT_TRUE(attributes.isValid());

        ASSERT_EQ("16", attrValue("System/MaxChannel"));
    }

    void longTest(const QString& data)
    {
        attributes = HanwhaAttributes(data, nx::network::http::StatusCode::ok);
        ASSERT_TRUE(attributes.isValid());

        ASSERT_EQ("True",  attrValue("System/ISPVersion"));
        ASSERT_EQ("False", attrValue("System/PTZBoardVersion"));
        ASSERT_EQ("False", attrValue("System/InterfaceBoardVersion"));
        ASSERT_EQ("True",  attrValue("System/ConfigRestore"));
        ASSERT_EQ("1",     attrValue("System/NICCount"));
        ASSERT_EQ("10",    attrValue("Network/MaxIPv4Filter"));
        ASSERT_EQ("1",     attrValue("Media/MaxAudioInput/0"));
        ASSERT_EQ("1",     attrValue("Media/MaxAudioOutput/0"));
        ASSERT_EQ("3", attrValue("Media/MaxAudioInput/1"));
        ASSERT_EQ("3", attrValue("Media/MaxAudioOutput/1"));
        ASSERT_EQ("commonValue", attrValue("Media/CommonLimit"));
        ASSERT_EQ("True",  attrValue("Media/DeviceAudioOutput/0"));
        ASSERT_EQ("", attrValue("Media/DeviceAudioOutput/1"));
        ASSERT_EQ("", attrValue("Media/DeviceAudioOutput"));
    }
};

static const char kShortXml[] =
R"xml(
<group name="System">
    <category name="Property"></category>
    <category name="Support"></category>
    <category name="Limit">
        <attribute name="MaxChannel" type="int" value="16" accesslevel="user"/>
    </category>
</group>
)xml";

static const char kLongXml[] =
R"xml(<?xml version="1.0" encoding="UTF-8"?>
<capabilities xmlns="http://www.samsungtechwin.com/AttributesSchema">
    <attributes>
        <group name="System">
            <category name="Property">
                <attribute accesslevel="guest" value="True" type="bool" name="ISPVersion"/>
                <attribute accesslevel="guest" value="False" type="bool" name="PTZBoardVersion"/>
                <attribute accesslevel="guest" value="False" type="bool" name="InterfaceBoardVersion"/>
                <attribute/>
            </category>
            <category name="Support">
                <attribute accesslevel="admin" value="True" type="bool" name="ConfigRestore"/>
            </category>
            <category/>
            <category name="Limit">
                <attribute accesslevel="guest" value="1" type="int" name="NICCount"/>
            </category>
        </group>
        <group name="Network">
            <category name="Limit">
                <attribute accesslevel="guest" value="10" type="int" name="MaxIPv4Filter"/>
            </category>
        </group>
        <group name="Media">
            <category name="Limit">
                <channel number="0">
                    <attribute accesslevel="guest" value="1" type="int" name="MaxAudioInput"/>
                    <attribute accesslevel="guest" value="1" type="int" name="MaxAudioOutput"/>
                </channel>
                <channel number="1">
                    <attribute accesslevel="guest" value="3" type="int" name="MaxAudioInput"/>
                    <attribute accesslevel="guest" value="3" type="int" name="MaxAudioOutput"/>
                </channel>
                <attribute accesslevel="guest" value="commonValue" type="string"  name="CommonLimit"/>
            </category>
            <category name="Support">
                <channel number="0">
                    <attribute accesslevel="suser" value="True" type="bool" name="DeviceAudioOutput"/>
                </channel>
            </category>
        </group>
    </attributes>
    <cgis>
        <cgi name="system">
            <submenu name="deviceinfo">
                <action accesslevel="guest" name="view">
                    <parameter response="true" request="false" name="Model">
                        <dataType>
                            <string maxlen="31"/>
                        </dataType>
                    </parameter>
                </action>
            </submenu>
        </cgi>
    </cgis>
</capabilities>
)xml";

static QString removeWhiteSpace(const QString& s)
{
    auto lines = s.split('\n');
    for (auto& line: lines)
        line = line.trimmed();
    return lines.join("");
}

TEST_F(HanwhaAttributesParser, ShortFormated) { shortTest(kShortXml); }
TEST_F(HanwhaAttributesParser, ShortCompact) { shortTest(removeWhiteSpace(kShortXml)); }

TEST_F(HanwhaAttributesParser, LongFormated) { longTest(kLongXml); }
TEST_F(HanwhaAttributesParser, LongCompact) { longTest(removeWhiteSpace(kLongXml)); }

} // namespace test
} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)