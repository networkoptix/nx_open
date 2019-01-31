#include <sstream>
#include <gtest/gtest.h>

#include "media_server/serverutil.h"

TEST(AutodetectHttpContentType, main)
{
    using namespace nx::vms::server;

    QByteArray kJsonText =
        "{ \"data\": [{ \"keys\": [\"Sony|SNC-VM772R\"] }] }";
    QByteArray result = Utils::autoDetectHttpContentType(kJsonText);
    ASSERT_EQ(result, QByteArray("application/json"));

    QByteArray kInvalidJsonText =
        "{ \"data\": [ \"keys\": [\"Sony|SNC-VM772R\"] }] }";
    result = Utils::autoDetectHttpContentType(kInvalidJsonText);
    ASSERT_EQ(result, QByteArray("text/plain"));

    QByteArray kXMLText =
        "<resources> <oem name = \"Axis\"> <resource name = \"AXISP7216Group1\" public = \"AXIS_ENCODER\"/> </oem> </resources>";
    result = Utils::autoDetectHttpContentType(kXMLText);
    ASSERT_EQ(result, QByteArray("application/xml"));

    QByteArray kInvalidXMLText =
        "<resources <oem name = \"Axis\"> <resource name = \"AXISP7216Group1\" public = \"AXIS_ENCODER\"/> </oem> </resources>";
    result = Utils::autoDetectHttpContentType(kInvalidXMLText);
    ASSERT_EQ(result, QByteArray("text/plain"));

    QByteArray kHTMLText =
        "<html lang=\"en_US\"> <body> test </body> </html>";
    result = Utils::autoDetectHttpContentType(kHTMLText);
    ASSERT_EQ(result, QByteArray("text/html; charset=utf-8"));
    QByteArray kHTMLText2 =
        "<\"some string with <html> tag\" <html lang=\"en_US\"> <body> test </body> </html>";
    result = Utils::autoDetectHttpContentType(kHTMLText2);
    ASSERT_EQ(result, QByteArray("text/html; charset=utf-8"));

    QByteArray kInvalidHTMLText =
        "{ \"data\": [{ \"keys\": [\"<html></html>\"] }] }";
    result = Utils::autoDetectHttpContentType(kInvalidHTMLText);
    ASSERT_EQ(result, QByteArray("application/json"));

    return;
}
