#include <sstream>
#include <gtest/gtest.h>

#include "media_server/serverutil.h"

TEST(AutodetectHttpContentType, main)
{
    QByteArray kJsonText =
        "{ \"data\": [{ \"keys\": [\"Sony|SNC-VM772R\"] }] }";
    QByteArray result = autoDetectHttpContentType(kJsonText);
    ASSERT_EQ(result, QByteArray("application/json"));

    QByteArray kInvalidJsonText =
        "{ \"data\": [ \"keys\": [\"Sony|SNC-VM772R\"] }] }";
    result = autoDetectHttpContentType(kInvalidJsonText);
    ASSERT_EQ(result, QByteArray("text/plain"));

    QByteArray kXMLText =
        "<resources> <oem name = \"Axis\"> <resource name = \"AXISP7216Group1\" public = \"AXIS_ENCODER\"/> </oem> </resources>";
    result = autoDetectHttpContentType(kXMLText);
    ASSERT_EQ(result, QByteArray("application/xml"));

    QByteArray kInvalidXMLText =
        "<resources <oem name = \"Axis\"> <resource name = \"AXISP7216Group1\" public = \"AXIS_ENCODER\"/> </oem> </resources>";
    result = autoDetectHttpContentType(kInvalidXMLText);
    ASSERT_EQ(result, QByteArray("text/plain"));

    QByteArray kHTMLText =
        "<html lang=\"en_US\"> <body> test </body> </html>";
    result = autoDetectHttpContentType(kHTMLText);
    ASSERT_EQ(result, QByteArray("text/html; charset=utf-8"));
    QByteArray kHTMLText2 =
        "<\"some string with <html>\ tag\" <html lang=\"en_US\"> <body> test </body> </html>";
    result = autoDetectHttpContentType(kHTMLText2);
    ASSERT_EQ(result, QByteArray("text/html; charset=utf-8"));

    QByteArray kInvalidHTMLText =
        "{ \"data\": [{ \"keys\": [\"<html></html>\"] }] }";
    result = autoDetectHttpContentType(kInvalidHTMLText);
    ASSERT_EQ(result, QByteArray("application/json"));

    return;
}
