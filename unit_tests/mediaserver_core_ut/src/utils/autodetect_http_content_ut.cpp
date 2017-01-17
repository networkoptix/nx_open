#include <sstream>
#include <gtest/gtest.h>

#include "media_server/serverutil.h"

TEST(AutodetectHttpContentType, main)
{
    QByteArray kJsonText =
        "{ \"data\": [{ \"keys\": [\"Sony|SNC-VM772R\"] }] }";
    QByteArray result = autoDetectHttpContentType(kJsonText);
    ASSERT_EQ(result, QByteArray("application/json"));

    QByteArray kinvalidJsonText =
        "{ \"data\": [ \"keys\": [\"Sony|SNC-VM772R\"] }] }";
    result = autoDetectHttpContentType(kJsonText);
    ASSERT_EQ(result, QByteArray("application/json"));

    QByteArray kXMLText =
        "<resources> <oem name = \"Axis\"> <resource name = \"AXISP7216Group1\" public = \"AXIS_ENCODER\"/> </oem> </resources>";
    result = autoDetectHttpContentType(kXMLText);
    ASSERT_EQ(result, QByteArray("application/xml"));

    QByteArray kInvalidXMLText =
        "<resources <oem name = \"Axis\"> <resource name = \"AXISP7216Group1\" public = \"AXIS_ENCODER\"/> </oem> </resources>";
    result = autoDetectHttpContentType(kInvalidXMLText);
    ASSERT_EQ(result, QByteArray("text/plain"));

    QByteArray kHTMLext =
        "\"<html>\" <html lang=\"en_US\"> <body> test </body> </html>";
    result = autoDetectHttpContentType(kHTMLext);
    ASSERT_EQ(result, QByteArray("text/html; charset=utf-8"));

    QByteArray kInvalidHTMLext =
        "\"<html>\" lang=\"en_US\"> <body> test </body> </html>";
    result = autoDetectHttpContentType(kInvalidHTMLext);
    ASSERT_EQ(result, QByteArray("text/plain"));

    return;
}
