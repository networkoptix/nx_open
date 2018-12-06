#include <gtest/gtest.h>

#include <plugins/resource/hanwha/hanwha_cgi_parameters.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace vms::server {
namespace plugins {
namespace test {

struct HanwhaCgiParser: ::testing::Test
{
    void parse(const char* data)
    {
        HanwhaCgiParameters parameters(data, network::http::StatusCode::ok);
        ASSERT_TRUE(parameters.isValid());

        auto stringParam = parameters.parameter("system/deviceinfo/view/stringParam");
        ASSERT_TRUE(stringParam);
        auto intParam = parameters.parameter("system/deviceinfo/view/intParam");
        ASSERT_TRUE(intParam);
        auto floatParam = parameters.parameter("system/deviceinfo/view/floatParam");
        ASSERT_TRUE(floatParam);
        auto boolParam = parameters.parameter("system/deviceinfo/view/boolParam");
        ASSERT_TRUE(boolParam);
        auto enumParam = parameters.parameter("system/deviceinfo/view/enumParam");
        ASSERT_TRUE(enumParam);

        ASSERT_EQ(9, stringParam->maxLength());
        ASSERT_EQ(HanwhaCgiParameterType::string, stringParam->type());

        ASSERT_EQ(5, intParam->min());
        ASSERT_EQ(15, intParam->max());
        ASSERT_EQ(HanwhaCgiParameterType::integer, intParam->type());

        ASSERT_FLOAT_EQ( 0.1, floatParam->floatMin());
        ASSERT_FLOAT_EQ(176.0, floatParam->floatMax());
        ASSERT_EQ(HanwhaCgiParameterType::floating, floatParam->type());

        ASSERT_EQ("True", boolParam->trueValue());
        ASSERT_EQ("False", boolParam->falseValue());
        ASSERT_EQ("False", boolParam->falseValue());
        ASSERT_EQ(HanwhaCgiParameterType::boolean, boolParam->type());

        ASSERT_TRUE(enumParam->possibleValues().contains("entry0"));
        ASSERT_TRUE(enumParam->possibleValues().contains("entry1"));
        ASSERT_TRUE(enumParam->possibleValues().contains("entry2"));
        ASSERT_TRUE(enumParam->possibleValues().contains("entry3"));
        ASSERT_FALSE(enumParam->possibleValues().contains("entry4"));

        ASSERT_FALSE(enumParam->isRequestParameter());
        ASSERT_TRUE(enumParam->isResponseParameter());
        ASSERT_EQ(HanwhaCgiParameterType::enumeration, enumParam->type());

        auto stringParam2 = parameters.parameter("system2/deviceinfo2/view2/stringParam2");
        ASSERT_EQ(10, stringParam2->maxLength());
    }
};

static const char kShortXml[] =
R"xml(<?xml version="1.0" encoding="UTF-8"?>
<cgis>
    <cgi name="Unused0"/>
    <cgi name="system">
        <submenu name="Unused"/>
        <submenu name="deviceinfo">
            <action name="Unused"/>
            <action name="view" accesslevel="user">
                <parameter name="stringParam" request="false" response="true">
                    <dataType>
                        <string maxlen="9"/>
                    </dataType>
                </parameter>
                <parameter name="intParam" request="false" response="true">
                    <dataType>
                        <int min="5" max="15"/>
                    </dataType>
                </parameter>
                <parameter name="floatParam" request="false" response="true">
                    <dataType>
                        <float min="0.100000" max="176.000000"/>
                    </dataType>
                </parameter>
                <parameter name="boolParam" request="false" response="true">
                    <dataType>
                        <bool true="True" false="False"/>
                    </dataType>
                </parameter>
                <parameter name="enumParam" request="false" response="true">
                    <dataType>
                        <csv>
                            <entry value="entry0"/>
                            <entry value="entry1"/>
                        </csv>
                        <enum>
                            <entry value="entry2"/>
                            <entry value="entry3"/>
                        </enum>
                    </dataType>
                </parameter>
            </action>
        </submenu>
    </cgi>
    <cgi name="system2">
        <submenu name="deviceinfo2">
            <action name="view2" accesslevel="user">
                <parameter name="stringParam2" request="false" response="true">
                    <dataType>
                        <string maxlen="10"/>
                    </dataType>
                </parameter>
            </action>
        </submenu>
    </cgi>
</cgis>
)xml";

static QByteArray removeWhiteSpace(const QByteArray& s)
{
    auto lines = s.split('\n');
    for (auto& line : lines)
        line = line.trimmed();
    return lines.join("");
}

TEST_F(HanwhaCgiParser, ShortFormated) { parse(kShortXml); }
TEST_F(HanwhaCgiParser, ShortCompact)  { parse(removeWhiteSpace(QByteArray(kShortXml))); }

} // namespace test
} // namespace plugins
} // namespace vms::server
} // namespace nx

