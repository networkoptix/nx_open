#include <gtest/gtest.h>

#include <plugins/resource/hanwha/hanwha_attributes.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {
namespace test {

const char testData1[] =
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

const char testData2[] =
R"xml(<group name="System"><category name="Property"></category><category name="Support"></category><category name="Limit"><attribute name="MaxChannel" type="int" value="16" accesslevel="user"/></category></group>)xml";

TEST(HanwhaAttributesParser, main)
{
    HanwhaAttributes attributes;
    auto attrValue = [&](const QString& path)
    {
        auto result = attributes.attribute<QString>(path);
        return result ? *result : QString();
    };

    attributes = HanwhaAttributes(testData1, nx_http::StatusCode::ok);
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
    ASSERT_EQ("True",  attrValue("Media/DeviceAudioOutput/0"));
    ASSERT_EQ("", attrValue("Media/DeviceAudioOutput/1"));
    ASSERT_EQ("", attrValue("Media/DeviceAudioOutput"));
    ASSERT_TRUE(attributes.isValid());

    attributes = HanwhaAttributes(testData2, nx_http::StatusCode::ok);
    ASSERT_EQ("16", attrValue("System/MaxChannel"));
    ASSERT_TRUE(attributes.isValid());
}

} // namespace test
} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
