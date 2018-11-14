#include <gtest/gtest.h>

#include <nx/cloud/db/data/access_rules.h>
#include <nx/cloud/db/stree/cdb_ns.h>

namespace nx::cloud::db {
namespace data {
namespace test {

class AccessRestrictions:
    public ::testing::Test
{
protected:
    void parseRules(const std::string& rulesStr)
    {
        ASSERT_TRUE(m_accessRestrictions.parse(m_nameset, rulesStr));
    }

    void assertRequestIsAuthorized(
        const std::string& path,
        const std::vector<std::tuple<attr::Value, std::string>>& fields = {})
    {
        ASSERT_TRUE(m_accessRestrictions.authorize(prepareQueryData(path, fields)));
    }

    void assertRequestIsNotAuthorized(
        const std::string& path,
        const std::vector<std::tuple<attr::Value, std::string>>& fields = {})
    {
        ASSERT_FALSE(m_accessRestrictions.authorize(prepareQueryData(path, fields)));
    }

    void assertParseAndToStringIsSymmetric(const std::string& rulesStr)
    {
        data::AccessRestrictions accessRestrictions;

        ASSERT_TRUE(accessRestrictions.parse(m_nameset, rulesStr));
        ASSERT_EQ(rulesStr, accessRestrictions.toString(m_nameset));
    }

private:
    const CdbAttrNameSet m_nameset;
    data::AccessRestrictions m_accessRestrictions;

    utils::stree::ResourceContainer prepareQueryData(
        const std::string& path,
        const std::vector<std::tuple<attr::Value, std::string>>& fields = {})
    {
        utils::stree::ResourceContainer rc;
        rc.put(attr::requestPath, QString::fromStdString(path));
        for (const auto& field: fields)
            rc.put(std::get<0>(field), QString::fromStdString(std::get<1>(field)));
        return rc;
    }
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

    // Rules with fields.
    assertParseAndToStringIsSymmetric("+/allowed/request1;+ha1,+user.name:-/allowed/request2");
    assertParseAndToStringIsSymmetric("+/allowed/request1;-ha1");
}

TEST_F(AccessRestrictions, allow_rule_with_denied_fields)
{
    parseRules("+/allowed/request1;-ha1");

    assertRequestIsAuthorized("/allowed/request1");
    assertRequestIsNotAuthorized("/allowed/request1", {{ attr::ha1, "value" }});
}

TEST_F(AccessRestrictions, allow_rule_with_required_fields)
{
    parseRules("+/allowed/request1;+user.name");

    assertRequestIsNotAuthorized("/allowed/request1", {{ attr::ha1, "value" }});
    assertRequestIsAuthorized("/allowed/request1", {{ attr::userName, "value" }});
}

TEST_F(AccessRestrictions, deny_rule_with_required_fields)
{
    parseRules("-/allowed/request1;+ha1");

    assertRequestIsNotAuthorized("/allowed/request1", {{ attr::ha1, "value" }});
    assertRequestIsAuthorized("/allowed/request1", {{ attr::userName, "value" }});
}

TEST_F(AccessRestrictions, deny_rule_with_multiple_required_fields)
{
    parseRules("-/allowed/request1;+ha1,+user.name");

    assertRequestIsAuthorized("/allowed/request1");
    assertRequestIsNotAuthorized("/allowed/request1", {{ attr::ha1, "value" }});
    assertRequestIsNotAuthorized("/allowed/request1", {{ attr::userName, "value" }});
}

} // namespace test
} // namespace data
} // namespace nx::cloud::db
