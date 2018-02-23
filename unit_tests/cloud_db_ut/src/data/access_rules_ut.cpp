#include <gtest/gtest.h>

#include <nx/cloud/cdb/data/account_data.h>
#include <nx/cloud/cdb/stree/cdb_ns.h>

namespace nx {
namespace cdb {
namespace data {
namespace test {

class AccessRestrictions:
    public ::testing::Test
{
protected:
    void parseRules(const std::string& rulesStr)
    {
        ASSERT_TRUE(m_accessRestrictions.parse(rulesStr));
    }

    void assertRequestIsAuthorized(const std::string& path)
    {
        utils::stree::ResourceContainer rc;
        rc.put(attr::requestPath, QString::fromStdString(path));
        ASSERT_TRUE(m_accessRestrictions.authorize(rc));
    }

    void assertRequestIsNotAuthorized(const std::string& path)
    {
        utils::stree::ResourceContainer rc;
        rc.put(attr::requestPath, QString::fromStdString(path));
        ASSERT_FALSE(m_accessRestrictions.authorize(rc));
    }

    void assertParseAndToStringIsSymmetric(const std::string& rulesStr)
    {
        data::AccessRestrictions accessRestrictions;

        ASSERT_TRUE(accessRestrictions.parse(rulesStr));
        ASSERT_EQ(rulesStr, accessRestrictions.toString());
    }

private:
    data::AccessRestrictions m_accessRestrictions;
};

TEST_F(AccessRestrictions, allowed_request_is_authorized)
{
    parseRules("+/allowed/request1:+/allowed/request2");

    assertRequestIsAuthorized("/allowed/request1");
    assertRequestIsAuthorized("/allowed/request2");
}

TEST_F(AccessRestrictions, allow_rules_present_unknown_request_is_not_authorized)
{
    parseRules("+/allowed/request1:+/allowed/request2");

    assertRequestIsNotAuthorized("/allowed/request3");
}

TEST_F(AccessRestrictions, denied_request_is_not_authorized)
{
    parseRules("-/allowed/request1");

    assertRequestIsNotAuthorized("/allowed/request1");
}

TEST_F(AccessRestrictions, not_denied_request_is_authorized)
{
    parseRules("-/allowed/request1");

    assertRequestIsAuthorized("/allowed/request2");
}

TEST_F(AccessRestrictions, allow_rules_are_checked_first)
{
    parseRules("-/allowed/request1:+/allowed/request1");

    assertRequestIsAuthorized("/allowed/request1");
}

TEST_F(AccessRestrictions, deny_rules_are_ignored_if_allow_rules_are_present)
{
    parseRules("-/allowed/request1:+/allowed/request1");

    assertRequestIsNotAuthorized("/allowed/request2");
}

TEST_F(AccessRestrictions, parse_toString_are_symmetric)
{
    assertParseAndToStringIsSymmetric("-/allowed/request1");
    assertParseAndToStringIsSymmetric("+/allowed/request1");
    assertParseAndToStringIsSymmetric("+/allowed/request1:+/allowed/request2");
    assertParseAndToStringIsSymmetric("-/allowed/request1:-/allowed/request2");
    assertParseAndToStringIsSymmetric("+/allowed/request1:-/allowed/request2");
    assertParseAndToStringIsSymmetric("");
}

} // namespace test
} // namespace data
} // namespace cdb
} // namespace nx
