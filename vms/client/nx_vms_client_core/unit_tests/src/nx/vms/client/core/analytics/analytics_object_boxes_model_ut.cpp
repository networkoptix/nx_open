// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <algorithm>
#include <list>
#include <map>
#include <memory>

#include <gtest/gtest.h>

#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtTest/QAbstractItemModelTester>
#include <QtTest/QSignalSpy>

#include <analytics/db/abstract_object_type_dictionary.h>
#include <core/resource/resource.h>
#include <nx/vms/client/core/analytics/analytics_object_boxes_model.h>
#include <nx/vms/client/core/analytics/object_appearance_provider.h>
#include <nx/vms/client/core/analytics/object_tracker.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/media/abstract_analytics_metadata_provider.h>

namespace nx::vms::client::core {
namespace test {

using namespace std::chrono;
using namespace nx::common::metadata;
using analytics::ObjectTracker;

namespace {

const QRectF kRectangle(0.1, 0.1, 0.2, 0.2);
const QString kPersonTypeId = "nx.base.Person";
const QString kCarTypeId = "nx.base.Car";

/** Sets the object color explicitly, so the default appearance provider doesn't pick one. */
const Attribute kColorAttribute{"nx.sys.color", "#123456"};

class MetadataProviderMock: public AbstractAnalyticsMetadataProvider
{
public:
    virtual ObjectMetadataPacketPtr metadata(microseconds timestamp, int channel) const override
    {
        const auto packets = metadataRange(timestamp, timestamp, channel);
        return packets.empty() ? ObjectMetadataPacketPtr() : packets.front();
    }

    virtual std::list<ObjectMetadataPacketPtr> metadataRange(
        microseconds startTimestamp, microseconds endTimestamp, int channel) const override
    {
        std::list<ObjectMetadataPacketPtr> result;
        const auto it = m_packetsByChannel.find(channel);
        if (it == m_packetsByChannel.end())
            return result;

        for (const auto& packet: it->second)
        {
            const microseconds timestamp(packet->timestampUs);
            if (timestamp >= startTimestamp && timestamp <= endTimestamp)
                result.push_back(packet);
        }
        return result;
    }

    void addPacket(const ObjectMetadataPacketPtr& packet, int channel)
    {
        auto& packets = m_packetsByChannel[channel];
        const auto position = std::upper_bound(packets.begin(),
            packets.end(),
            packet,
            [](const auto& left, const auto& right)
            { return left->timestampUs < right->timestampUs; });
        packets.insert(position, packet);
    }

private:
    std::map<int, std::list<ObjectMetadataPacketPtr>> m_packetsByChannel;
};

class AppearanceProviderMock: public analytics::AbstractObjectAppearanceProvider
{
public:
    explicit AppearanceProviderMock(QString titleSuffix = {}):
        m_titleSuffix(std::move(titleSuffix))
    {
    }

    virtual QColor color(const ObjectMetadata& object) const override
    {
        return colorOf(QString::fromStdString(object.typeId));
    }

    virtual QString title(const ObjectMetadata& object) const override
    {
        return titleOf(QString::fromStdString(object.typeId)) + m_titleSuffix;
    }

    static QColor colorOf(const QString& typeId)
    {
        return typeId == kCarTypeId ? QColor(Qt::blue) : QColor(Qt::red);
    }

    static QString titleOf(const QString& typeId) { return typeId.section('.', -1); }

private:
    const QString m_titleSuffix;
};

class ObjectTypeDictionaryMock: public nx::analytics::db::AbstractObjectTypeDictionary
{
public:
    virtual std::optional<std::string> idToName(const std::string& id) const override
    {
        return QString::fromStdString(id).section('.', -1).toStdString();
    }
};

} // namespace

class TestAnalyticsObjectBoxesModel: public AnalyticsObjectBoxesModel
{
public:
    using AnalyticsObjectBoxesModel::setAppearanceProvider;
    using AnalyticsObjectBoxesModel::setObjectTypeDictionary;
};

class AnalyticsObjectBoxesModelTest: public ::testing::Test
{
protected:
    struct ObjectData
    {
        nx::Uuid trackId;
        QRectF boundingBox = kRectangle;
        QString typeId = kPersonTypeId;
        Attributes attributes;
    };

    virtual void SetUp() override
    {
        modelTester = std::make_unique<QAbstractItemModelTester>(
            &model, QAbstractItemModelTester::FailureReportingMode::Fatal);

        model.setAppearanceProvider(std::make_shared<AppearanceProviderMock>());
        model.setObjectTypeDictionary(std::make_shared<ObjectTypeDictionaryMock>());
        model.setMetadataProvider(metadataProvider);
    }

    void givenPacket(milliseconds timestamp, std::vector<ObjectData> objects, int channel = 0)
    {
        metadataProvider->addPacket(makePacket(timestamp, objects), channel);
    }

    void givenPacketOfOtherMetadataProvider(
        milliseconds timestamp, std::vector<ObjectData> objects)
    {
        otherMetadataProvider->addPacket(makePacket(timestamp, objects), /*channel*/ 0);
    }

    void givenPosition(milliseconds position) { whenPositionIs(position); }

    void givenModelWithoutMetadataProvider() { model.setMetadataProvider({}); }

    void givenResource() { model.setResource(QnResourcePtr(new QnResource())); }

    void givenDefaultAppearanceProviderAndDictionary()
    {
        // The default appearance provider takes the colors from the application context.
        appContext = std::make_unique<ApplicationContext>(ApplicationContext::Mode::unitTests,
            Qn::SerializationFormat::json,
            nx::vms::api::PeerType::mobileClient);

        model.setAppearanceProvider({});
        model.setObjectTypeDictionary({});
    }

    static ObjectMetadataPacketPtr makePacket(
        milliseconds timestamp, const std::vector<ObjectData>& objects)
    {
        auto packet = std::make_shared<ObjectMetadataPacket>();
        packet->timestampUs = microseconds(timestamp).count();

        for (const auto& data: objects)
        {
            ObjectMetadata object;
            object.trackId = data.trackId;
            object.boundingBox = data.boundingBox;
            object.typeId = data.typeId.toStdString();
            object.attributes = data.attributes;
            packet->objectMetadataList.push_back(object);
        }
        return packet;
    }

    void givenFilterTrackId(const nx::Uuid& trackId) { model.setFilterTrackId(trackId); }

    void givenFilterRect(const QRectF& rect) { model.setFilterRect(rect); }

    void givenFilterText(const QString& text) { model.setFilterText(text); }

    void givenAppearanceProvider(const QString& titleSuffix)
    {
        whenAppearanceProviderIsReplaced(titleSuffix);
    }

    void givenSignalsAreWatched()
    {
        rowsInsertedSpy = std::make_unique<QSignalSpy>(&model, &QAbstractItemModel::rowsInserted);
        rowsRemovedSpy = std::make_unique<QSignalSpy>(&model, &QAbstractItemModel::rowsRemoved);
        dataChangedSpy = std::make_unique<QSignalSpy>(&model, &QAbstractItemModel::dataChanged);
        modelResetSpy = std::make_unique<QSignalSpy>(&model, &QAbstractItemModel::modelReset);
    }

    void whenPositionIs(milliseconds position) { model.setPosition(position.count()); }

    void whenMetadataProviderIsSet() { model.setMetadataProvider(metadataProvider); }

    void whenMetadataProviderIsReplaced() { model.setMetadataProvider(otherMetadataProvider); }

    void whenMetadataProviderIsRemoved() { model.setMetadataProvider({}); }

    void whenResourceIsChanged() { model.setResource(QnResourcePtr(new QnResource())); }

    void whenChannelIs(int channel) { model.setChannel(channel); }

    void whenRefreshed() { model.refresh(); }

    void whenFilterTrackIdIs(const nx::Uuid& trackId) { model.setFilterTrackId(trackId); }

    void whenFilterRectIs(const QRectF& rect) { model.setFilterRect(rect); }

    void whenFilterTextIs(const QString& text) { model.setFilterText(text); }

    void whenAppearanceProviderIsReplaced(const QString& titleSuffix)
    {
        model.setAppearanceProvider(std::make_shared<AppearanceProviderMock>(titleSuffix));
    }

    void thenThereAreNoObjects() const { EXPECT_EQ(0, model.rowCount()); }

    void thenObjectCountIs(int count) const { EXPECT_EQ(count, model.rowCount()); }

    void thenObjectsAre(const std::vector<nx::Uuid>& trackIds) const
    {
        std::vector<nx::Uuid> actual;
        for (int row = 0; row < model.rowCount(); ++row)
            actual.push_back(trackIdAt(row));

        EXPECT_EQ(trackIds, actual);
    }

    void thenObjectTypeIs(int row, const QString& typeId) const
    {
        EXPECT_EQ(typeId, value(row, AnalyticsObjectBoxesModel::ObjectTypeIdRole).toString());
    }

    void thenRectangleIs(int row, const QRectF& expected) const
    {
        const auto actual = value(row, AnalyticsObjectBoxesModel::RectRole).toRectF();
        EXPECT_NEAR(expected.x(), actual.x(), kPrecision);
        EXPECT_NEAR(expected.y(), actual.y(), kPrecision);
        EXPECT_NEAR(expected.width(), actual.width(), kPrecision);
        EXPECT_NEAR(expected.height(), actual.height(), kPrecision);
    }

    void thenObjectIsPoint(int row, bool isPoint) const
    {
        EXPECT_EQ(isPoint, value(row, AnalyticsObjectBoxesModel::IsPointRole).toBool());
    }

    void thenColorIs(int row, const QColor& color) const
    {
        EXPECT_EQ(color, value(row, AnalyticsObjectBoxesModel::ColorRole).value<QColor>());
    }

    void thenTitleIs(int row, const QString& title) const
    {
        EXPECT_EQ(title, value(row, AnalyticsObjectBoxesModel::TitleRole).toString());
    }

    void thenRowsWereInserted(int first, int last) const
    {
        ASSERT_EQ(1, rowsInsertedSpy->count());
        EXPECT_EQ(first, rowsInsertedSpy->at(0).at(1).toInt());
        EXPECT_EQ(last, rowsInsertedSpy->at(0).at(2).toInt());
    }

    void thenNoRowsWereInserted() const { EXPECT_EQ(0, rowsInsertedSpy->count()); }

    void thenRowWasRemoved(int row) const { thenRowsWereRemoved(row, row); }

    void thenRowsWereRemoved(int first, int last) const
    {
        ASSERT_EQ(1, rowsRemovedSpy->count());
        EXPECT_EQ(first, rowsRemovedSpy->at(0).at(1).toInt());
        EXPECT_EQ(last, rowsRemovedSpy->at(0).at(2).toInt());
    }

    void thenNoRowsWereRemoved() const { EXPECT_EQ(0, rowsRemovedSpy->count()); }

    void thenRowDataChanged(int row) const
    {
        ASSERT_EQ(1, dataChangedSpy->count());
        EXPECT_EQ(row, dataChangedSpy->at(0).at(0).toModelIndex().row());
        EXPECT_EQ(row, dataChangedSpy->at(0).at(1).toModelIndex().row());
    }

    void thenNoDataChanged() const { EXPECT_EQ(0, dataChangedSpy->count()); }

    void thenModelWasReset() const { EXPECT_EQ(1, modelResetSpy->count()); }

    void thenModelWasNotReset() const { EXPECT_EQ(0, modelResetSpy->count()); }

    void whenRoleNamesAreRead() { roleNames = model.roleNames(); }

    void thenRoleNamesAre(const QHash<int, QByteArray>& expected) const
    {
        EXPECT_EQ(expected, roleNames);
    }

private:
    QVariant value(int row, int role) const { return model.data(model.index(row), role); }

    nx::Uuid trackIdAt(int row) const
    {
        return value(row, AnalyticsObjectBoxesModel::TrackIdRole).value<nx::Uuid>();
    }

protected:
    static constexpr qreal kPrecision = 1e-9;

    const nx::Uuid trackA = nx::Uuid::createUuid();
    const nx::Uuid trackB = nx::Uuid::createUuid();
    const nx::Uuid trackC = nx::Uuid::createUuid();
    const nx::Uuid trackD = nx::Uuid::createUuid();

    const QSharedPointer<MetadataProviderMock> metadataProvider{new MetadataProviderMock()};
    const QSharedPointer<MetadataProviderMock> otherMetadataProvider{new MetadataProviderMock()};
    TestAnalyticsObjectBoxesModel model;
    std::unique_ptr<QAbstractItemModelTester> modelTester;
    std::unique_ptr<ApplicationContext> appContext;

    std::unique_ptr<QSignalSpy> rowsInsertedSpy;
    std::unique_ptr<QSignalSpy> rowsRemovedSpy;
    std::unique_ptr<QSignalSpy> dataChangedSpy;
    std::unique_ptr<QSignalSpy> modelResetSpy;

    QHash<int, QByteArray> roleNames;
};

// Object lifetime.

TEST_F(AnalyticsObjectBoxesModelTest, objectIsNotShownBeforeItsFirstPacket)
{
    givenPacket(10000ms, {{trackA}});

    whenPositionIs(9900ms);

    thenThereAreNoObjects();
}

TEST_F(AnalyticsObjectBoxesModelTest, objectIsShownAtItsPacketTimestamp)
{
    givenPacket(10000ms, {{trackA}});

    whenPositionIs(10000ms);

    thenObjectsAre({trackA});
}

TEST_F(AnalyticsObjectBoxesModelTest, objectIsHiddenWhenItsLifetimeEnds)
{
    givenPacket(10000ms, {{trackA}});
    givenPosition(10000ms);

    whenPositionIs(10000ms + kMinimalObjectDuration + 1ms);

    thenThereAreNoObjects();
}

TEST_F(AnalyticsObjectBoxesModelTest, objectsAtCurrentPositionAreShownWhenMetadataProviderIsSet)
{
    givenModelWithoutMetadataProvider();
    givenPacket(10000ms, {{trackA}});
    givenPosition(10000ms);

    whenMetadataProviderIsSet();

    thenObjectsAre({trackA});
}

// Object data.

TEST_F(AnalyticsObjectBoxesModelTest, rolesHaveNamesUsedInQml)
{
    whenRoleNamesAreRead();

    thenRoleNamesAre({
        {AnalyticsObjectBoxesModel::TrackIdRole, "trackId"},
        {AnalyticsObjectBoxesModel::ObjectTypeIdRole, "objectTypeId"},
        {AnalyticsObjectBoxesModel::TitleRole, "title"},
        {AnalyticsObjectBoxesModel::ColorRole, "color"},
        {AnalyticsObjectBoxesModel::RectRole, "rect"},
        {AnalyticsObjectBoxesModel::IsPointRole, "isPoint"},
    });
}

TEST_F(AnalyticsObjectBoxesModelTest, rowDescribesObject)
{
    givenPacket(10000ms, {{.trackId = trackA, .typeId = kCarTypeId}});

    whenPositionIs(10000ms);

    thenObjectTypeIs(0, kCarTypeId);
    thenRectangleIs(0, kRectangle);
    thenObjectIsPoint(0, false);
    thenColorIs(0, AppearanceProviderMock::colorOf(kCarTypeId));
    thenTitleIs(0, "Car");
}

TEST_F(AnalyticsObjectBoxesModelTest, objectWithShowAsPointAttributeIsPoint)
{
    givenPacket(10000ms, {{.trackId = trackA, .attributes = {{"nx.sys.showAsPoint", "true"}}}});

    whenPositionIs(10000ms);

    thenObjectIsPoint(0, true);
}

TEST_F(AnalyticsObjectBoxesModelTest, rectangleIsInterpolatedBetweenPackets)
{
    givenPacket(10000ms, {{.trackId = trackA, .boundingBox = QRectF(0.0, 0.0, 0.2, 0.2)}});
    givenPacket(11000ms, {{.trackId = trackA, .boundingBox = QRectF(0.4, 0.2, 0.4, 0.2)}});

    whenPositionIs(10250ms);

    thenRectangleIs(0, QRectF(0.1, 0.05, 0.25, 0.2));
}

TEST_F(AnalyticsObjectBoxesModelTest, rectangleStickingOutIsMovedIntoFrame)
{
    givenPacket(10000ms, {{.trackId = trackA, .boundingBox = QRectF(0.9, -0.1, 0.2, 0.2)}});

    whenPositionIs(10000ms);

    thenRectangleIs(0, QRectF(0.8, 0.0, 0.2, 0.2));
}

TEST_F(AnalyticsObjectBoxesModelTest, objectWithInvalidRectangleIsNotShown)
{
    givenPacket(10000ms, {{.trackId = trackA, .boundingBox = QRectF(0.5, 0.5, -0.1, -0.1)}});

    whenPositionIs(10000ms);

    thenThereAreNoObjects();
}

TEST_F(AnalyticsObjectBoxesModelTest, objectsOfOtherChannelsAreNotShown)
{
    givenPacket(10000ms, {{trackA}}, /*channel*/ 1);

    whenPositionIs(10000ms);

    thenThereAreNoObjects();
}

TEST_F(AnalyticsObjectBoxesModelTest, objectsOfSelectedChannelAreShown)
{
    givenPacket(10000ms, {{trackA}}, /*channel*/ 0);
    givenPacket(10000ms, {{trackB}}, /*channel*/ 1);
    givenPosition(10000ms);

    whenChannelIs(1);

    thenObjectsAre({trackB});
}

// Incremental updates.

TEST_F(AnalyticsObjectBoxesModelTest, movingObjectKeepsItsRow)
{
    givenPacket(10000ms, {{.trackId = trackA, .boundingBox = QRectF(0.0, 0.0, 0.2, 0.2)}});
    givenPacket(10100ms, {{.trackId = trackA, .boundingBox = QRectF(0.2, 0.0, 0.2, 0.2)}});
    givenPosition(10000ms);
    givenSignalsAreWatched();

    whenPositionIs(10050ms);

    thenObjectsAre({trackA});
    thenNoRowsWereInserted();
    thenNoRowsWereRemoved();
    thenModelWasNotReset();
    thenRowDataChanged(0);
}

TEST_F(AnalyticsObjectBoxesModelTest, unchangedObjectDoesNotEmitDataChanged)
{
    givenPacket(10000ms, {{trackA}});
    givenPosition(10000ms);
    givenSignalsAreWatched();

    whenPositionIs(10050ms);

    thenObjectsAre({trackA});
    thenNoDataChanged();
}

TEST_F(AnalyticsObjectBoxesModelTest, appearedObjectIsAppendedAfterExistingOnes)
{
    givenPacket(10000ms, {{trackA}});
    givenPacket(10100ms, {{trackB}});
    givenPosition(10000ms);
    givenSignalsAreWatched();

    whenPositionIs(10100ms);

    thenObjectsAre({trackA, trackB});
    thenRowsWereInserted(1, 1);
    thenNoRowsWereRemoved();
}

TEST_F(AnalyticsObjectBoxesModelTest, vanishedObjectRowIsRemovedAndOthersAreKept)
{
    givenPacket(10000ms, {{trackA}});
    givenPacket(10100ms, {{trackB}});
    givenPosition(10000ms);
    givenPosition(10100ms);
    givenSignalsAreWatched();

    whenPositionIs(10250ms);

    thenObjectsAre({trackB});
    thenRowWasRemoved(0);
    thenNoRowsWereInserted();
}

TEST_F(AnalyticsObjectBoxesModelTest, adjacentVanishedObjectRowsAreRemovedTogether)
{
    givenPacket(10000ms, {{trackA}});
    givenPacket(10050ms, {{trackB}});
    givenPacket(10100ms, {{trackC}});
    givenPosition(10000ms);
    givenPosition(10050ms);
    givenPosition(10100ms);
    givenSignalsAreWatched();

    whenPositionIs(10260ms);

    thenObjectsAre({trackC});
    thenRowsWereRemoved(0, 1);
    thenNoRowsWereInserted();
}

TEST_F(AnalyticsObjectBoxesModelTest, keptObjectRowIsUpdatedWhileLaterRowIsRemoved)
{
    givenPacket(10000ms, {{.trackId = trackA, .boundingBox = QRectF(0.0, 0.0, 0.2, 0.2)}});
    givenPacket(10400ms, {{.trackId = trackA, .boundingBox = QRectF(0.4, 0.0, 0.2, 0.2)}});
    givenPacket(10050ms, {{trackB}});
    givenPosition(10000ms);
    givenPosition(10050ms);
    givenSignalsAreWatched();

    whenPositionIs(10260ms);

    thenObjectsAre({trackA});
    thenRowWasRemoved(1);
    thenRowDataChanged(0);
}

// Metadata provider and resource changes.

TEST_F(AnalyticsObjectBoxesModelTest, replacedMetadataProviderReplacesObjects)
{
    givenPacket(10000ms, {{trackA}});
    givenPacketOfOtherMetadataProvider(10000ms, {{trackB}});
    givenPosition(10000ms);
    givenSignalsAreWatched();

    whenMetadataProviderIsReplaced();

    thenObjectsAre({trackB});
    thenModelWasReset();
}

TEST_F(AnalyticsObjectBoxesModelTest, removedMetadataProviderClearsObjects)
{
    givenPacket(10000ms, {{trackA}});
    givenPosition(10000ms);

    whenMetadataProviderIsRemoved();

    thenThereAreNoObjects();
}

TEST_F(AnalyticsObjectBoxesModelTest, objectsAreNotShownWithoutMetadataProvider)
{
    givenModelWithoutMetadataProvider();

    whenPositionIs(10000ms);

    thenThereAreNoObjects();
}

TEST_F(AnalyticsObjectBoxesModelTest, objectsAreFetchedOnResourceChange)
{
    givenResource();
    givenPacket(10000ms, {{trackA}});
    givenPosition(10000ms);

    whenResourceIsChanged();

    thenObjectsAre({trackA});
}

// The media player replaces the metadata provider on the same resource change, before or after
// the model gets the new resource. Either way, only the new metadata provider objects remain.
TEST_F(AnalyticsObjectBoxesModelTest, resourceChangeAfterProviderReplacementShowsItsObjects)
{
    givenPacket(10000ms, {{trackA}});
    givenPacketOfOtherMetadataProvider(10000ms, {{trackB}});
    givenPosition(10000ms);

    whenMetadataProviderIsReplaced();
    whenResourceIsChanged();

    thenObjectsAre({trackB});
}

TEST_F(AnalyticsObjectBoxesModelTest, providerReplacementAfterResourceChangeShowsItsObjects)
{
    givenPacket(10000ms, {{trackA}});
    givenPacketOfOtherMetadataProvider(10000ms, {{trackB}});
    givenPosition(10000ms);

    whenResourceIsChanged();
    whenMetadataProviderIsReplaced();

    thenObjectsAre({trackB});
}

TEST_F(AnalyticsObjectBoxesModelTest, objectsAreFetchedOnPositionChangeAfterResourceChange)
{
    givenPacket(10000ms, {{trackA}});
    givenPosition(10000ms);
    givenResource();

    whenPositionIs(10050ms);

    thenObjectsAre({trackA});
}

// Refreshing.

TEST_F(AnalyticsObjectBoxesModelTest, objectsReceivedAtUnchangedPositionAreShownOnRefresh)
{
    givenPosition(10000ms);
    givenPacket(10000ms, {{trackA}});

    whenRefreshed();

    thenObjectsAre({trackA});
}

TEST_F(AnalyticsObjectBoxesModelTest, refreshWithoutNewMetadataKeepsRowsUnchanged)
{
    givenPacket(10000ms, {{trackA}, {trackB}});
    givenPosition(10000ms);
    givenSignalsAreWatched();

    whenRefreshed();

    thenObjectCountIs(2);
    thenNoRowsWereInserted();
    thenNoRowsWereRemoved();
    thenNoDataChanged();
    thenModelWasNotReset();
}

TEST_F(AnalyticsObjectBoxesModelTest, refreshWithoutMetadataProviderShowsNoObjects)
{
    givenModelWithoutMetadataProvider();
    givenPacket(10000ms, {{trackA}});
    givenPosition(10000ms);

    whenRefreshed();

    thenThereAreNoObjects();
}

// Filtering.

TEST_F(AnalyticsObjectBoxesModelTest, filterTrackIdShowsOnlyThatTrack)
{
    givenPacket(10000ms, {{trackA}, {trackB}});
    givenPosition(10000ms);

    whenFilterTrackIdIs(trackB);

    thenObjectsAre({trackB});
}

TEST_F(AnalyticsObjectBoxesModelTest, nullFilterTrackIdShowsAllTracks)
{
    givenPacket(10000ms, {{trackA}, {trackB}});
    givenFilterTrackId(trackB);
    givenPosition(10000ms);

    whenFilterTrackIdIs(nx::Uuid());

    thenObjectCountIs(2);
}

TEST_F(AnalyticsObjectBoxesModelTest, filterRectHidesObjectsOutsideIt)
{
    givenPacket(10000ms,
        {{.trackId = trackA, .boundingBox = QRectF(0.1, 0.1, 0.1, 0.1)},
            {.trackId = trackB, .boundingBox = QRectF(0.7, 0.7, 0.1, 0.1)}});
    givenPosition(10000ms);

    whenFilterRectIs(QRectF(0.5, 0.5, 0.5, 0.5));

    thenObjectsAre({trackB});
}

TEST_F(AnalyticsObjectBoxesModelTest, filterRectShowsPartiallyIntersectingObjects)
{
    givenPacket(10000ms, {{.trackId = trackA, .boundingBox = QRectF(0.4, 0.4, 0.2, 0.2)}});
    givenPosition(10000ms);

    whenFilterRectIs(QRectF(0.5, 0.5, 0.5, 0.5));

    thenObjectsAre({trackA});
}

TEST_F(AnalyticsObjectBoxesModelTest, invalidFilterRectShowsAllObjects)
{
    givenPacket(10000ms,
        {{.trackId = trackA, .boundingBox = QRectF(0.1, 0.1, 0.1, 0.1)},
            {.trackId = trackB, .boundingBox = QRectF(0.7, 0.7, 0.1, 0.1)}});
    givenFilterRect(QRectF(0.5, 0.5, 0.5, 0.5));
    givenPosition(10000ms);

    whenFilterRectIs(QRectF());

    thenObjectCountIs(2);
}

TEST_F(AnalyticsObjectBoxesModelTest, filterTextMatchesObjectTypeName)
{
    givenPacket(10000ms,
        {{.trackId = trackA, .typeId = kPersonTypeId}, {.trackId = trackB, .typeId = kCarTypeId}});
    givenPosition(10000ms);

    whenFilterTextIs("car");

    thenObjectsAre({trackB});
}

TEST_F(AnalyticsObjectBoxesModelTest, filterTextDoesNotMatchDisplayedTitle)
{
    givenAppearanceProvider(" (parked)");
    givenPacket(10000ms, {{.trackId = trackA, .typeId = kCarTypeId}});
    givenPosition(10000ms);

    whenFilterTextIs("parked");

    thenThereAreNoObjects();
}

TEST_F(AnalyticsObjectBoxesModelTest, filterTextMatchesObjectAttributeValue)
{
    givenPacket(10000ms,
        {{.trackId = trackA, .attributes = {{"Color", "Red"}}},
            {.trackId = trackB, .attributes = {{"Color", "Blue"}}}});
    givenPosition(10000ms);

    whenFilterTextIs("blue");

    thenObjectsAre({trackB});
}

TEST_F(AnalyticsObjectBoxesModelTest, filterTextMatchesAttributeCondition)
{
    givenPacket(10000ms,
        {{.trackId = trackA, .attributes = {{"Color", "Red"}}},
            {.trackId = trackB, .attributes = {{"Color", "Blue"}}}});
    givenPosition(10000ms);

    whenFilterTextIs("Color:Red");

    thenObjectsAre({trackA});
}

TEST_F(AnalyticsObjectBoxesModelTest, filterTextMatchingNothingHidesAllObjects)
{
    givenPacket(10000ms, {{trackA}, {trackB}});
    givenPosition(10000ms);

    whenFilterTextIs("bicycle");

    thenThereAreNoObjects();
}

TEST_F(AnalyticsObjectBoxesModelTest, emptyFilterTextShowsAllObjects)
{
    givenPacket(10000ms, {{trackA}, {trackB}});
    givenFilterText("bicycle");
    givenPosition(10000ms);

    whenFilterTextIs("");

    thenObjectCountIs(2);
}

TEST_F(AnalyticsObjectBoxesModelTest, allFiltersMustMatch)
{
    const QRectF kInside(0.6, 0.6, 0.1, 0.1);
    const QRectF kOutside(0.1, 0.1, 0.1, 0.1);
    givenPacket(10000ms,
        {{.trackId = trackA, .boundingBox = kInside, .typeId = kCarTypeId},
            {.trackId = trackB, .boundingBox = kOutside, .typeId = kCarTypeId},
            {.trackId = trackC, .boundingBox = kInside, .typeId = kPersonTypeId},
            {.trackId = trackD, .boundingBox = kInside, .typeId = kCarTypeId}});
    givenPosition(10000ms);
    givenFilterRect(QRectF(0.5, 0.5, 0.5, 0.5));
    givenFilterText("car");

    whenFilterTrackIdIs(trackA);

    thenObjectsAre({trackA});
}

TEST_F(AnalyticsObjectBoxesModelTest, filteredOutObjectReappearsInItsOwnRowWhenFilterIsCleared)
{
    givenPacket(10000ms, {{trackA}, {trackB}});
    givenPosition(10000ms);
    givenFilterTrackId(trackA);
    givenSignalsAreWatched();

    whenFilterTrackIdIs(nx::Uuid());

    thenObjectCountIs(2);
    thenRowsWereInserted(1, 1);
    thenNoRowsWereRemoved();
}

TEST_F(AnalyticsObjectBoxesModelTest, defaultFilteringWithoutResourceMatchesAttributes)
{
    givenDefaultAppearanceProviderAndDictionary();
    givenPacket(10000ms,
        {{.trackId = trackA, .attributes = {kColorAttribute, {"Color", "Red"}}},
            {.trackId = trackB, .attributes = {kColorAttribute, {"Color", "Blue"}}}});
    givenPosition(10000ms);

    whenFilterTextIs("blue");

    thenObjectsAre({trackB});
}

TEST_F(AnalyticsObjectBoxesModelTest, defaultFilteringWithoutResourceDoesNotMatchTypeNames)
{
    givenDefaultAppearanceProviderAndDictionary();
    givenPacket(10000ms, {{.trackId = trackA, .attributes = {kColorAttribute}}});
    givenPosition(10000ms);

    whenFilterTextIs("person");

    thenThereAreNoObjects();
}

// Appearance.

TEST_F(AnalyticsObjectBoxesModelTest, replacedAppearanceProviderIsAppliedToShownObjects)
{
    givenPacket(10000ms, {{trackA}});
    givenPosition(10000ms);

    whenAppearanceProviderIsReplaced(" (new)");

    thenTitleIs(0, "Person (new)");
}

// Passing the metadata provider through QML.

class AnalyticsObjectBoxesModelQmlTest: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        qRegisterMetaType<AbstractAnalyticsMetadataProviderPtr>();
        AnalyticsObjectBoxesModel::registerQmlType();
    }

    void givenModelBoundToAnotherInQml()
    {
        static const QByteArray kComponentData = R"(
            import QtQml
            import nx.vms.client.core

            QtObject
            {
                property AnalyticsObjectBoxesModel origin: AnalyticsObjectBoxesModel
                {
                    id: originModel
                }

                property AnalyticsObjectBoxesModel target: AnalyticsObjectBoxesModel
                {
                    metadataProvider: originModel.metadataProvider
                }
            }
        )";

        QQmlComponent component(&engine);
        component.setData(kComponentData, QUrl());
        ASSERT_TRUE(component.isReady()) << component.errorString().toStdString();

        root.reset(component.create());
        ASSERT_TRUE(root);
    }

    void whenOriginModelGetsMetadataProvider()
    {
        modelAt("origin")->setMetadataProvider(metadataProvider);
    }

    void thenBoundModelHasSameMetadataProvider() const
    {
        EXPECT_EQ(metadataProvider, modelAt("target")->metadataProvider());
    }

private:
    AnalyticsObjectBoxesModel* modelAt(const char* propertyName) const
    {
        return root->property(propertyName).value<AnalyticsObjectBoxesModel*>();
    }

protected:
    const AbstractAnalyticsMetadataProviderPtr metadataProvider{new MetadataProviderMock()};
    QQmlEngine engine;
    std::unique_ptr<QObject> root;
};

TEST_F(AnalyticsObjectBoxesModelQmlTest, metadataProviderIsPassedThroughBinding)
{
    givenModelBoundToAnotherInQml();

    whenOriginModelGetsMetadataProvider();

    thenBoundModelHasSameMetadataProvider();
}

} // namespace test
} // namespace nx::vms::client::core
