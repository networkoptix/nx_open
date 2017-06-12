/**********************************************************
* Aug 18, 2016
* akolesnikov
***********************************************************/

#include <cstring>
#include <string>

#include <gtest/gtest.h>

#include <QtCore/QBuffer>

#include <nx/utils/stree/stree_manager.h>


namespace nx {
namespace utils {
namespace stree {

using ::nx::utils::stree::AbstractNode;
using ::nx::utils::stree::ResourceContainer;
using ::nx::utils::stree::ResourceNameSet;
using ::nx::utils::stree::StreeManager;

class StreeTest
:
    public ::testing::Test
{
public:
    enum Attributes
    {
        intAttr = 1,
        strAttr,
        outAttr,
    };

    StreeTest()
    {
        m_nameSet.registerResource(intAttr, "intAttr", QVariant::Int);
        m_nameSet.registerResource(strAttr, "strAttr", QVariant::String);
        m_nameSet.registerResource(outAttr, "outAttr", QVariant::String);
    }

protected:
    bool prepareTree(const char* xmlDataStr)
    {
        auto xmlData = QByteArray::fromRawData(xmlDataStr, std::strlen(xmlDataStr));
        QBuffer xmlDataSource(&xmlData);
        m_streeRoot = StreeManager::loadStree(&xmlDataSource, m_nameSet);
        return m_streeRoot != nullptr;
    }

    const std::unique_ptr<AbstractNode>& streeRoot() const
    {
        return m_streeRoot;
    }

private:
    ResourceNameSet m_nameSet;
    std::unique_ptr<AbstractNode> m_streeRoot;
};

class StreeRangeMatch
:
    public StreeTest
{
protected:
    std::string getOutAttr(std::string strAttrVal)
    {
        ResourceContainer intputData;
        intputData.put(StreeTest::strAttr, QString::fromStdString(strAttrVal));
        return getOutAttr(intputData);
    }
        
    std::string getOutAttr(int intAttrVal)
    {
        ResourceContainer intputData;
        intputData.put(StreeTest::intAttr, intAttrVal);
        return getOutAttr(intputData);
    }

    std::string getOutAttr(const ResourceContainer& intputData)
    {
        ResourceContainer outputData;
        streeRoot()->get(::nx::utils::stree::MultiSourceResourceReader(intputData, outputData), &outputData);
        if (auto outVal = outputData.get<std::string>(StreeTest::outAttr))
            return *outVal;
        return std::string();
    }
};

TEST_F(StreeRangeMatch, intValue)
{
    const char* testXmlData =
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<sequence>\n"
        "    <set resName=\"outAttr\" resValue=\"default\"/>\n"
        "    <condition resName=\"intAttr\" matchType=\"range\">\n"
        "        <set value=\"0-9\" resName=\"outAttr\" resValue=\"first\"/>\n"
        "        <set value=\"0-9\" resName=\"outAttr\" resValue=\"xxx\"/>\n"
        "        <set value=\"20-30\" resName=\"outAttr\" resValue=\"second\"/>\n"
        "    </condition>\n"
        "</sequence>\n";

    ASSERT_TRUE(prepareTree(testXmlData));

    ASSERT_EQ("first", getOutAttr(0));
    ASSERT_EQ("first", getOutAttr(9));
    ASSERT_EQ("first", getOutAttr(5));

    ASSERT_EQ("default", getOutAttr(-1));
    ASSERT_EQ("default", getOutAttr(10));
    ASSERT_EQ("default", getOutAttr(19));

    ASSERT_EQ("second", getOutAttr(20));
    ASSERT_EQ("second", getOutAttr(30));
    ASSERT_EQ("second", getOutAttr(25));

    ASSERT_EQ("default", getOutAttr(31));
    ASSERT_EQ("default", getOutAttr(100));
}

TEST_F(StreeRangeMatch, strValue)
{
    const char* testXmlData =
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<sequence>\n"
        "    <set resName=\"outAttr\" resValue=\"default\"/>\n"
        "    <condition resName=\"strAttr\" matchType=\"range\">\n"
        "        <set value=\"aa-ad\" resName=\"outAttr\" resValue=\"first\"/>\n"
        "        <set value=\"bb-cc\" resName=\"outAttr\" resValue=\"second\"/>\n"
        "    </condition>\n"
        "</sequence>\n";

    ASSERT_TRUE(prepareTree(testXmlData));

    ASSERT_EQ("default", getOutAttr("a0"));

    ASSERT_EQ("first", getOutAttr("aa"));
    ASSERT_EQ("first", getOutAttr("ab"));
    ASSERT_EQ("first", getOutAttr("ad"));

    ASSERT_EQ("default", getOutAttr("ae"));
    ASSERT_EQ("default", getOutAttr("ba"));

    ASSERT_EQ("second", getOutAttr("bb"));
    ASSERT_EQ("second", getOutAttr("cb"));
    ASSERT_EQ("second", getOutAttr("cc"));

    ASSERT_EQ("default", getOutAttr("cd"));
    ASSERT_EQ("default", getOutAttr("d"));
    ASSERT_EQ("default", getOutAttr("ff"));
}

}   // namespace stree
}   // namespace utils
}   // namespace nx
