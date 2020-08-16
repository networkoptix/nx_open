#include <gtest/gtest.h>

#include <analytics/db/text_search_utils.h>


namespace nx::analytics::db {

std::ostream& operator<<(std::ostream& os, const TextSearchCondition& val)
{
    os << (int) val.type << ". ";

    switch (val.type)
    {
        case ConditionType::attributePresenceCheck:
            os << val.name.toStdString();
            break;

        case ConditionType::attributeValueMatch:
            os << val.name.toStdString() << ":" << val.value.toStdString();
            break;

        case ConditionType::textMatch:
            os << val.text.toStdString();
            break;
    }

    return os;
}

} // namespace nx::analytics::db

namespace nx::analytics::db::test {

class TextSearchExpressionParser:
    public ::testing::Test
{
protected:
    std::vector<TextSearchCondition> parse(const QString& text)
    {
        db::UserTextSearchExpressionParser parser;
        return parser.parse(text);
    }
};

TEST_F(TextSearchExpressionParser, parse)
{
    ASSERT_EQ(
        std::vector<TextSearchCondition>({
            TextMatch("red")}),
        parse("red*"));

    ASSERT_EQ(
        std::vector<TextSearchCondition>({
            AttributeValueMatch("Wheel Size", "20")}),
        parse("\"Wheel Size\": 20"));

    ASSERT_EQ(
        std::vector<TextSearchCondition>({
            TextMatch("Wheel"), AttributeValueMatch("Size", "20")}),
        parse("  Wheel Size: 20"));

    ASSERT_EQ(
        std::vector<TextSearchCondition>({
            AttributeValueMatch("car.brand.model", "Aston Martin")}),
        parse("car.brand.model:  \"Aston Martin\""));

    ASSERT_EQ(
        std::vector<TextSearchCondition>({
            AttributeValueMatch("car.brand.model", "Aston"), TextMatch("red")}),
        parse("car.brand.model:  Aston red  "));

    ASSERT_EQ(
        std::vector<TextSearchCondition>({
            AttributeValueMatch("car.brand.model", "Aston Martin"),
            AttributeValueMatch("car.color", "red")}),
        parse("car.brand.model : \"Aston Martin\" car.color :red  "));
}

TEST_F(TextSearchExpressionParser, attribute_presence_check)
{
    ASSERT_EQ(
        std::vector<TextSearchCondition>({
            AttributePresenceCheck("foo")}),
        parse("$foo"));

    ASSERT_EQ(
        std::vector<TextSearchCondition>({
            AttributePresenceCheck("foo")}),
        parse("foo:"));

    ASSERT_EQ(
        std::vector<TextSearchCondition>({
            AttributePresenceCheck("foo") }),
            parse("foo="));

    ASSERT_EQ(
        std::vector<TextSearchCondition>({
            AttributePresenceCheck("foo") }),
            parse("foo :  "));

    // Just ignoring misplaced :.
    ASSERT_EQ(
        std::vector<TextSearchCondition>({
            TextMatch("text")}),
            parse(": text"));

    ASSERT_EQ(
        std::vector<TextSearchCondition>({
            AttributeValueMatch("foo", "car.brand.model"),
            TextMatch("Aston Martin"),
            TextMatch("text")}),
        parse(": foo:  car.brand.model : \"Aston Martin\"  text"));

    ASSERT_EQ(
        std::vector<TextSearchCondition>({
            AttributeValueMatch("foo", "car.brand.model"),
            TextMatch("Aston Martin"),
            TextMatch("text") }),
            parse(": foo:  car.brand.model = \"Aston Martin\"  text"));
}

TEST_F(TextSearchExpressionParser, unescaping)
{
    ASSERT_EQ(
        std::vector<TextSearchCondition>({
            TextMatch("red flag ")}),
        parse("red\\ flag\\ "));

    ASSERT_EQ(
        std::vector<TextSearchCondition>({
            TextMatch("red*") }),
            parse("red\\*"));

    ASSERT_EQ(
        std::vector<TextSearchCondition>({
            TextMatch("$nx.network") }),
            parse("\\$nx\\.network"));

    ASSERT_EQ(
        std::vector<TextSearchCondition>({
            TextMatch("color:red") }),
            parse("color\\:red"));

    ASSERT_EQ(
        std::vector<TextSearchCondition>({
            TextMatch("color=red") }),
            parse("color\\=red"));

    ASSERT_EQ(
        std::vector<TextSearchCondition>({
            TextMatch("\"red\\ flag\"") }),
            parse("\\\"red\\\\\\ flag\\\""));
}

} // namespace nx::analytics::db::test
