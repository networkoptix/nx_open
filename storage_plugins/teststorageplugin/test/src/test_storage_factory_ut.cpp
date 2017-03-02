#include <gtest/gtest.h>
#include <test_storage_factory.h>
#include "config_sample.h"

class TestStorageFactoryHelper : public TestStorageFactory
{
public:
    std::string getConfigPath() const { return m_path; }

private:
    virtual bool readConfig(const std::string& path, std::string* outContent)
    {
        m_path = path;
        outContent->assign(kTestJson);

        return true;
    }

    std::string m_path;
};

TEST(TestStorageFactoryTest, main)
{
    TestStorageFactoryHelper factory;
    int ecode;

    /* auto generated config name */
    auto storage = factory.createStorage("test://storage1/some/path", &ecode);
    ASSERT_EQ(ecode, nx_spl::error::NoError);
    ASSERT_TRUE(storage);
    ASSERT_TRUE(factory.getConfigPath().find("storage1.cfg") != std::string::npos);

    storage->releaseRef();

    /* custom config name */
    storage = factory.createStorage("test://storage1/some/path?config=myConfig.cfg", &ecode);
    ASSERT_EQ(ecode, nx_spl::error::NoError);
    ASSERT_TRUE(storage);
    ASSERT_TRUE(factory.getConfigPath().find("myConfig.cfg") != std::string::npos);

    storage->releaseRef();
}

TEST(TestStorageFactoryTest, wrongUrl)
{
    TestStorageFactoryHelper factory;

    /* wrong scheme */
    ASSERT_FALSE(factory.createStorage("tes://storage1/some/path", nullptr));
}