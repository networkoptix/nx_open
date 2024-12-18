// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QString>

#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_grouping/resource_grouping.h>

namespace {

QString makeRawCompositeId(const QStringList& subIdList)
{
    using namespace nx::vms::client::desktop::entity_resource_tree::resource_grouping;
    return subIdList.join(kSubIdDelimiter);
}

} // namespace

namespace nx::vms::client::desktop {
namespace entity_resource_tree {
namespace resource_grouping {
namespace test {

class ResourceGrouping: public ::testing::Test
{
};

//-------------------------------------------------------------------------------------------------
// Test cases for getNewGroupSubId() function.
//-------------------------------------------------------------------------------------------------



//-------------------------------------------------------------------------------------------------
// Test cases for isValidSubId() function.
//-------------------------------------------------------------------------------------------------

TEST_F(ResourceGrouping, nullSubIdIsInvalid)
{
    const auto nullSubId = QString();
    EXPECT_TRUE(nullSubId.isNull());
    ASSERT_FALSE(isValidSubId(QString()));
}

TEST_F(ResourceGrouping, emptySubIdIsInvalid)
{
    const auto nullSubId = QString("");
    EXPECT_FALSE(nullSubId.isNull());
    ASSERT_FALSE(isValidSubId(QString("")));
}

TEST_F(ResourceGrouping, subIdWithLeadingWhitespaceIsInvalid)
{
    const QString validSubId("Group");
    EXPECT_TRUE(isValidSubId(validSubId));

    EXPECT_FALSE(isValidSubId(" " + validSubId));
    EXPECT_FALSE(isValidSubId("    " + validSubId));
    EXPECT_FALSE(isValidSubId(QChar::Nbsp + validSubId));
    EXPECT_FALSE(isValidSubId(QChar::LineFeed + validSubId));
    EXPECT_FALSE(isValidSubId(QChar::Tabulation + validSubId));
}

TEST_F(ResourceGrouping, subIdWithTrailingWhitespaceIsInvalid)
{
    const QString validSubId("Group");
    EXPECT_TRUE(isValidSubId(validSubId));

    EXPECT_FALSE(isValidSubId(validSubId + " "));
    EXPECT_FALSE(isValidSubId(validSubId + "    "));
    EXPECT_FALSE(isValidSubId(validSubId + QChar::Nbsp));
    EXPECT_FALSE(isValidSubId(validSubId + QChar::LineFeed));
    EXPECT_FALSE(isValidSubId(validSubId + QChar::Tabulation));
}

TEST_F(ResourceGrouping, subIdWithWhitespacesIsInvalid)
{
    ASSERT_FALSE(isValidSubId(" Group "));
}

TEST_F(ResourceGrouping, subIdWithWhitespacesInTheMiddleIsAllowed)
{
    QString subId("Group");
    subId.insert(2, QChar::Space);
    subId.insert(2, QChar::Nbsp);
    subId.insert(2, QChar::Tabulation);
    ASSERT_TRUE(isValidSubId(subId));
}

TEST_F(ResourceGrouping, subIdContainsLineFeedIsInvalid)
{
    QString subId("Group");
    subId.insert(2, QChar::LineFeed);
    ASSERT_FALSE(isValidSubId(subId));
}

TEST_F(ResourceGrouping, subIdExceedingLenthLimitIsInvalid)
{
    QString subId(kMaxSubIdLength + 1, 'G');
    ASSERT_FALSE(isValidSubId(subId));
}

TEST_F(ResourceGrouping, subIdAtLenthLimitIsValid)
{
    QString subId(kMaxSubIdLength, 'G');
    ASSERT_TRUE(isValidSubId(subId));
}

//-------------------------------------------------------------------------------------------------
// Test cases for fixupSubId() function.
//-------------------------------------------------------------------------------------------------



//-------------------------------------------------------------------------------------------------
// Test cases for getResourceTreeDisplayText() function.
//-------------------------------------------------------------------------------------------------

TEST_F(ResourceGrouping, displayTextIsLastSubId)
{
    const auto compositeId = makeRawCompositeId({"First", "Second", "Third"});
    ASSERT_EQ(getResourceTreeDisplayText(compositeId), "Third");
}

TEST_F(ResourceGrouping, displayTextForDimension1CompositeIdIsTheSameString)
{
    const auto compositeId = makeRawCompositeId({"First"});
    ASSERT_EQ(getResourceTreeDisplayText(compositeId), compositeId);
}

TEST_F(ResourceGrouping, displayTextForEmptyCompositeIdIsNullString)
{
    const auto compositeId = QString("");
    EXPECT_FALSE(compositeId.isNull());
    EXPECT_TRUE(compositeId.isEmpty());
    EXPECT_TRUE(getResourceTreeDisplayText(compositeId).isNull());
}

TEST_F(ResourceGrouping, displayTextForNullCompositeIdIsNullString)
{
    const auto compositeId = QString();
    EXPECT_TRUE(compositeId.isNull());
    EXPECT_TRUE(compositeId.isEmpty());
    EXPECT_TRUE(getResourceTreeDisplayText(compositeId).isNull());
}

//-------------------------------------------------------------------------------------------------
// Test cases for compositeIdDimension() function.
//-------------------------------------------------------------------------------------------------

TEST_F(ResourceGrouping, compositeIdDimensionEmptyPartsAtTheBeginning)
{
    auto compositeId = makeRawCompositeId({
        "",
        "Second",
        "Third",
        "Fourth",
        "Fifth",
        "Sixth",
        "Seventh, with many words",
        "Eighth"});
    ASSERT_EQ(compositeIdDimension(compositeId), 7);

    compositeId = makeRawCompositeId({
        "",
        "",
        "Third",
        "Fourth",
        "Fifth",
        "Sixth",
        "Seventh, with many words",
        "Eighth"});
    ASSERT_EQ(compositeIdDimension(compositeId), 6);
}

TEST_F(ResourceGrouping, compositeIdDimensionEmptyPartsInTheMiddle)
{
    const auto compositeId = makeRawCompositeId({
        "First",
        "Second",
        "",
        "Fourth",
        "Fifth",
        "",
        "",
        "Eighth"});
    ASSERT_EQ(compositeIdDimension(compositeId), 5);
}

TEST_F(ResourceGrouping, compositeIdDimensionEmptyPartsAtTheEnd)
{
    auto compositeId = makeRawCompositeId({
        "",
        "Second",
        "Third",
        "Fourth",
        "Fifth",
        "Sixth",
        "Seventh, with many words",
        "Eighth"});
    ASSERT_EQ(compositeIdDimension(compositeId), 7);

    compositeId = makeRawCompositeId({
        "",
        "",
        "Third",
        "Fourth",
        "Fifth",
        "Sixth",
        "Seventh, with many words",
        "Eighth"});
    ASSERT_EQ(compositeIdDimension(compositeId), 6);
}

TEST_F(ResourceGrouping, compositeIdDimensionNoEmptyParts)
{
    const auto compositeId = makeRawCompositeId({
        "First",
        "Second",
        "Third",
        "Fourth",
        "Fifth",
        "Sixth",
        "Seventh, with many words",
        "Eighth"});
    ASSERT_EQ(compositeIdDimension(compositeId), 8);
}

TEST_F(ResourceGrouping, compositeIdDimensionOnlyEmptyParts)
{
    const auto compositeId = makeRawCompositeId({"", "", "", "", ""});
    ASSERT_EQ(compositeIdDimension(compositeId), 0);
}

TEST_F(ResourceGrouping, compositeIdDimensionSingleSubId)
{
    const auto compositeId = makeRawCompositeId({"First"});
    ASSERT_EQ(compositeIdDimension(compositeId), 1);
}

TEST_F(ResourceGrouping, compositeIdDimensionSingleSubIdWithEmptyParts)
{
    const auto compositeId = makeRawCompositeId({"", "First", ""});
    ASSERT_EQ(compositeIdDimension(compositeId), 1);
}

TEST_F(ResourceGrouping, compositeIdDimensionOneCharSubId)
{
    const auto compositeId = makeRawCompositeId({"A"});
    ASSERT_EQ(compositeIdDimension(compositeId), 1);
}

TEST_F(ResourceGrouping, compositeIdDimensionOneCharSubIdOneEmptyPart)
{
    auto compositeId = makeRawCompositeId({"A", ""});
    ASSERT_EQ(compositeIdDimension(compositeId), 1);

    compositeId = makeRawCompositeId({"", "A"});
    ASSERT_EQ(compositeIdDimension(compositeId), 1);
}

TEST_F(ResourceGrouping, compositeIdDimensionOneCharSubIdTwoEmptyParts)
{
    auto compositeId = makeRawCompositeId({"A", "", ""});
    ASSERT_EQ(compositeIdDimension(compositeId), 1);

    compositeId = makeRawCompositeId({"", "A", ""});
    ASSERT_EQ(compositeIdDimension(compositeId), 1);

    compositeId = makeRawCompositeId({"", "", "A"});
    ASSERT_EQ(compositeIdDimension(compositeId), 1);
}

//-------------------------------------------------------------------------------------------------
// Test cases for trimCompositeId() function.
//-------------------------------------------------------------------------------------------------

TEST_F(ResourceGrouping, compositeIdTrimForZero)
{
    const auto compositeId = makeRawCompositeId({
        "First",
        "Second",
        "Third",
        "Fourth",
        "Fifth",
        "Sixth",
        "Seventh, with many words",
        "Eighth"});
    ASSERT_EQ(trimCompositeId(compositeId, 0), makeRawCompositeId({}));
}

TEST_F(ResourceGrouping, compositeIdTrimHalf)
{
    const auto compositeId = makeRawCompositeId({
        "First",
        "Second",
        "Third",
        "Fourth",
        "Fifth",
        "Sixth",
        "Seventh, with many words",
        "Eighth"});
    ASSERT_EQ(trimCompositeId(compositeId, 4),
        makeRawCompositeId({"First", "Second", "Third", "Fourth"}));
}

TEST_F(ResourceGrouping, compositeIdTrimToSameDimension)
{
    const auto compositeId = makeRawCompositeId({
        "First",
        "Second",
        "Third",
        "Fourth",
        "Fifth",
        "Sixth",
        "Seventh, with many words",
        "Eighth"});
    ASSERT_EQ(trimCompositeId(compositeId, 8), compositeId);
}

TEST_F(ResourceGrouping, compositeIdTrimToLargerDimension)
{
    const auto compositeId = makeRawCompositeId({
        "First",
        "Second",
        "Third",
        "Fourth"});
    ASSERT_EQ(trimCompositeId(compositeId, 8), compositeId);
}

TEST_F(ResourceGrouping, compositeIdTrimEmpty)
{
    ASSERT_EQ(trimCompositeId(QString(), 4), QString());
}

//-------------------------------------------------------------------------------------------------
// Test cases for cutCompositeIdFront() function.
//-------------------------------------------------------------------------------------------------

TEST_F(ResourceGrouping, compositeIdCutZeroSubIdsFromFront)
{
    const auto compositeId = makeRawCompositeId({
        "First",
        "Second",
        "Third",
        "Fourth",
        "Fifth",
        "Sixth",
        "Seventh, with many words",
        "Eighth"});
    auto compositeIdCopy = compositeId;
    cutCompositeIdFront(compositeIdCopy, 0);
    ASSERT_EQ(compositeIdCopy, compositeId);
}

TEST_F(ResourceGrouping, compositeIdCutSubIdsFromFrontOfMaximumDimensionId)
{
    auto compositeId = makeRawCompositeId({
        "First",
        "Second",
        "Third",
        "Fourth",
        "Fifth",
        "Sixth",
        "Seventh, with many words",
        "Eighth"});
    ASSERT_EQ(compositeIdDimension(compositeId), kUserDefinedGroupingDepth);
    cutCompositeIdFront(compositeId, 4);
    ASSERT_EQ(compositeId,
        makeRawCompositeId({"Fifth", "Sixth", "Seventh, with many words", "Eighth"}));
}

TEST_F(ResourceGrouping, compositeIdCutSubIdsFromFront)
{
    auto compositeId = makeRawCompositeId({
        "First",
        "Second",
        "Third",
        "Fourth"});
    cutCompositeIdFront(compositeId, 2);
    ASSERT_EQ(compositeId, makeRawCompositeId({"Third", "Fourth"}));
}

TEST_F(ResourceGrouping, compositeIdCutAllSubIdsFromFront)
{
    auto compositeId = makeRawCompositeId({
        "First",
        "Second",
        "Third",
        "Fourth",
        "Fifth",
        "Sixth",
        "Seventh, with many words",
        "Eighth"});
    cutCompositeIdFront(compositeId, 8);
    ASSERT_EQ(compositeId, makeRawCompositeId({}));
}

TEST_F(ResourceGrouping, compositeIdCutMoreSubIdsThanExist)
{
    auto compositeId = makeRawCompositeId({
        "First",
        "Second",
        "Third",
        "Fourth"});
    cutCompositeIdFront(compositeId, 6);
    ASSERT_EQ(compositeId, makeRawCompositeId({}));
}

TEST_F(ResourceGrouping, compositeIdCutMoreSubIdsThanExistCountEqualDimensionLimit)
{
    auto compositeId = makeRawCompositeId({
        "First",
        "Second",
        "Third",
        "Fourth"});
    cutCompositeIdFront(compositeId, kUserDefinedGroupingDepth);
    ASSERT_EQ(compositeId, makeRawCompositeId({}));
}

TEST_F(ResourceGrouping, compositeIdCutFromNull)
{
    auto emptyCompositeId = QString();
    cutCompositeIdFront(emptyCompositeId, 4);
    ASSERT_EQ(emptyCompositeId, QString());
}

//-------------------------------------------------------------------------------------------------
// Test cases for extractSubId() function.
//-------------------------------------------------------------------------------------------------

TEST_F(ResourceGrouping, compositeIdExtractFirstSubId)
{
    const auto compositeId = makeRawCompositeId({
        "First",
        "Second",
        "Third",
        "Fourth",
        "Fifth",
        "Sixth",
        "Seventh, with many words",
        "Eighth"});
    ASSERT_EQ(extractSubId(compositeId, 0), "First");
}

TEST_F(ResourceGrouping, compositeIdExtractMidSubId)
{
    const auto compositeId = makeRawCompositeId({
        "First",
        "Second",
        "Third",
        "Fourth",
        "Fifth",
        "Sixth",
        "Seventh, with many words",
        "Eighth"});
    ASSERT_EQ(extractSubId(compositeId, 3), "Fourth");
}

TEST_F(ResourceGrouping, compositeIdExtractLastSubId)
{
    const auto compositeId = makeRawCompositeId({
        "First",
        "Second",
        "Third",
        "Fourth",
        "Fifth",
        "Sixth",
        "Seventh, with many words",
        "Eighth"});
    ASSERT_EQ(extractSubId(compositeId, 7), "Eighth");
}

TEST_F(ResourceGrouping, compositeIdExtractOnlySubId)
{
    const auto compositeId = makeRawCompositeId({"First"});
    ASSERT_EQ(extractSubId(compositeId, 0), "First");
}

TEST_F(ResourceGrouping, compositeIdExtractOnlySingleCharSubId)
{
    const auto compositeId = makeRawCompositeId({"A"});
    ASSERT_EQ(extractSubId(compositeId, 0), "A");
}

//-------------------------------------------------------------------------------------------------
// Test cases for appendSubId() function.
//-------------------------------------------------------------------------------------------------

TEST_F(ResourceGrouping, appendSubId)
{
    ASSERT_EQ(appendSubId(
        makeRawCompositeId({"First", "Second", "Third", "Fourth"}), "Fifth"),
        makeRawCompositeId({"First", "Second", "Third", "Fourth", "Fifth"}));
}

TEST_F(ResourceGrouping, appendSubIdToEmptyCompositeId)
{
    ASSERT_EQ(appendSubId(
        makeRawCompositeId({}), "First"),
        makeRawCompositeId({"First"}));
}

TEST_F(ResourceGrouping, appendEmptySubIdToCompositeId)
{
    ASSERT_EQ(appendSubId(
        makeRawCompositeId({"First"}), ""),
        makeRawCompositeId({"First"}));
}

TEST_F(ResourceGrouping, appendEmptySubIdToCompositeIdDimensionOver1)
{
    ASSERT_EQ(appendSubId(
        makeRawCompositeId({"First", "Second"}), ""),
        makeRawCompositeId({"First", "Second"}));
}

TEST_F(ResourceGrouping, appendEmptySubIdToEmptyCompositeId)
{
    ASSERT_EQ(appendSubId(
        makeRawCompositeId({}), ""),
        makeRawCompositeId({}));
}

//-------------------------------------------------------------------------------------------------
// Test cases for insertSubId() function.
//-------------------------------------------------------------------------------------------------

TEST_F(ResourceGrouping, insertSubIdToNullId)
{
    const auto nullId = QString();
    const auto insertBefore = 0;
    ASSERT_EQ(insertSubId(nullId, "Inserted", insertBefore), "Inserted");
}

TEST_F(ResourceGrouping, insertBeforeZeroIndex)
{
    ASSERT_EQ(insertSubId(
        makeRawCompositeId({"First", "Second", "Third", "Fourth"}), "Inserted", 0),
        makeRawCompositeId({"Inserted", "First", "Second", "Third", "Fourth"}));
}

TEST_F(ResourceGrouping, insertBeforeFirstIndex)
{
    ASSERT_EQ(insertSubId(
        makeRawCompositeId({"First", "Second"}), "Inserted", 1),
        makeRawCompositeId({"First", "Inserted", "Second"}));
}

TEST_F(ResourceGrouping, insertAfterLastIndex)
{
    ASSERT_EQ(insertSubId(
        makeRawCompositeId({"First", "Second"}), "Inserted", 2),
        makeRawCompositeId({"First", "Second", "Inserted"}));
}

TEST_F(ResourceGrouping, insertEmpty)
{
    ASSERT_EQ(insertSubId(
        makeRawCompositeId({"First", "Second"}), "", 1),
        makeRawCompositeId({"First", "Second"}));
}

//-------------------------------------------------------------------------------------------------
// Test cases for removeSubId() function.
//-------------------------------------------------------------------------------------------------

TEST_F(ResourceGrouping, removeOnlySubId)
{
    ASSERT_EQ(removeSubId(
        makeRawCompositeId({"First"}), 0),
        makeRawCompositeId({}));
}

TEST_F(ResourceGrouping, removeOnlySingleCharSubId)
{
    ASSERT_EQ(removeSubId(
        makeRawCompositeId({"First"}), 0),
        makeRawCompositeId({}));
}

TEST_F(ResourceGrouping, removeFirstSubId)
{
    ASSERT_EQ(removeSubId(
        makeRawCompositeId({"First", "Second", "Third", "Fourth", "Fifth"}), 0),
        makeRawCompositeId({"Second", "Third", "Fourth", "Fifth"}));
}

TEST_F(ResourceGrouping, removeMidSubId)
{
    ASSERT_EQ(
        removeSubId(makeRawCompositeId({"First", "Second", "Third", "Fourth", "Fifth"}), 2),
        makeRawCompositeId({"First", "Second", "Fourth", "Fifth"}));
}

TEST_F(ResourceGrouping, removeLastSubId)
{
    ASSERT_EQ(removeSubId(
        makeRawCompositeId({"First", "Second", "Third", "Fourth", "Fifth"}), 4),
        makeRawCompositeId({"First", "Second", "Third", "Fourth"}));
}

//-------------------------------------------------------------------------------------------------
// Test cases for replaceSubId() function.
//-------------------------------------------------------------------------------------------------

TEST_F(ResourceGrouping, replaceOnlySubId)
{
    ASSERT_EQ(replaceSubId(
        makeRawCompositeId({"First"}), "Replaced", 0),
        makeRawCompositeId({"Replaced"}));
}

TEST_F(ResourceGrouping, replaceOnlySingleCharSubId)
{
    ASSERT_EQ(replaceSubId(
        makeRawCompositeId({"A"}), "B", 0),
        makeRawCompositeId({"B"}));
}

TEST_F(ResourceGrouping, replaceOnlySubindexWithEmpty)
{
    ASSERT_EQ(replaceSubId(
        makeRawCompositeId({"First"}), "", 0),
        makeRawCompositeId({}));
}

TEST_F(ResourceGrouping, replaceFirstSubId)
{
    ASSERT_EQ(replaceSubId(
        makeRawCompositeId({"First", "Second", "Third", "Fourth"}), "Replaced", 0),
        makeRawCompositeId({"Replaced", "Second", "Third", "Fourth"}));
}

TEST_F(ResourceGrouping, replaceFirstSubIdWithEmpty)
{
    ASSERT_EQ(replaceSubId(
        makeRawCompositeId({"First", "Second", "Third", "Fourth"}), "", 0),
        makeRawCompositeId({"Second", "Third", "Fourth"}));
}

TEST_F(ResourceGrouping, replaceMidSubId)
{
    ASSERT_EQ(replaceSubId(
        makeRawCompositeId({"First", "Second", "Third", "Fourth"}), "Replaced", 2),
        makeRawCompositeId({"First", "Second", "Replaced", "Fourth"}));
}

TEST_F(ResourceGrouping, replaceMidSubIdWithEmpty)
{
    ASSERT_EQ(replaceSubId(
        makeRawCompositeId({"First", "Second", "Third", "Fourth"}), "", 2),
        makeRawCompositeId({"First", "Second", "Fourth"}));
}

TEST_F(ResourceGrouping, replaceLastSubId)
{
    ASSERT_EQ(replaceSubId(
        makeRawCompositeId({"First", "Second", "Third", "Fourth"}), "Replaced", 3),
        makeRawCompositeId({"First", "Second", "Third", "Replaced"}));
}

TEST_F(ResourceGrouping, replaceLastSubIdWithEmpty)
{
    ASSERT_EQ(replaceSubId(
        makeRawCompositeId({"First", "Second", "Third", "Fourth"}), "", 3),
        makeRawCompositeId({"First", "Second", "Third"}));
}

} // namespace test
} // namespace resource_grouping
} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
