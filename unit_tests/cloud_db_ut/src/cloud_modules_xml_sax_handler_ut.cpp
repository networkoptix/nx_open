/**********************************************************
* Sep 9, 2015
* akolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include <QtCore/QBuffer>
#include <QtXml/QXmlSimpleReader>

#include <utils/network/cloud_connectivity/cloud_modules_xml_sax_handler.h>


namespace nx {
namespace cdb {
namespace cl {

TEST(CloudModulesXmlHandler, common)
{
    //< ? xml version = \"1.0\" encoding=\"utf-8\"?>  
    QByteArray testData( "                          \
        <modules>                                   \
        <cdb>                                       \
        <endpoint>10.0.2.95:3346</endpoint>         \
        <endpoint>10.0.2.101:3346</endpoint>        \
        </cdb>                                      \
        <hpm>                                       \
        <endpoint>10.0.2.102:123</endpoint>         \
        <endpoint>10.0.2.103:456</endpoint>         \
        </hpm>                                      \
        </modules>                                  \
        ");

    nx::cc::CloudModulesXmlHandler xmlHandler;

    QXmlSimpleReader reader;
    reader.setContentHandler(&xmlHandler);
    reader.setErrorHandler(&xmlHandler);

    QBuffer xmlBuffer(&testData);
    QXmlInputSource input(&xmlBuffer);
    ASSERT_TRUE(reader.parse(&input));

    const auto cdbEndpoints = xmlHandler.moduleUrls("cdb");
    ASSERT_EQ(cdbEndpoints.size(), 2);
    ASSERT_EQ(cdbEndpoints.front(), SocketAddress("10.0.2.95:3346"));
    ASSERT_EQ(*std::next(cdbEndpoints.begin(), 1), SocketAddress("10.0.2.101:3346"));

    const auto hpmEndpoints = xmlHandler.moduleUrls("hpm");
    ASSERT_EQ(hpmEndpoints.size(), 2);
    ASSERT_EQ(hpmEndpoints.front(), SocketAddress("10.0.2.102:123"));
    ASSERT_EQ(*std::next(hpmEndpoints.begin(), 1), SocketAddress("10.0.2.103:456"));
}

}   //cl
}   //cdb
}   //nx
