#include <gtest/gtest.h>

#include <transaction/transaction.h>

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::test {

TEST(Lexical, enumSerialization)
{
    ASSERT_EQ(
        "saveUser",
        QnLexical::serialized(ec2::ApiCommand::saveUser).toStdString());

    ASSERT_EQ(
        "Regular",
        QnLexical::serialized(ec2::TransactionType::Regular).toStdString());
}

TEST(Lexical, enumDeserialization)
{
    {
        auto value = QnLexical::deserialized<ec2::ApiCommand::Value>("saveUser");
        ASSERT_EQ(
            ec2::ApiCommand::saveUser,
            value);

        value = QnLexical::deserialized<ec2::ApiCommand::Value>(
            QString::number((int)ec2::ApiCommand::saveUser));
        ASSERT_EQ(
            ec2::ApiCommand::saveUser,
            value);
    }

    {
        auto value = QnLexical::deserialized<ec2::TransactionType::Value>("Regular");
        ASSERT_EQ(
            ec2::TransactionType::Regular,
            value);

        value = QnLexical::deserialized<ec2::TransactionType::Value>(
            QString::number((int)ec2::TransactionType::Regular));
        ASSERT_EQ(
            ec2::TransactionType::Regular,
            value);
    }
}

} // namespace nx::vms::api::test
