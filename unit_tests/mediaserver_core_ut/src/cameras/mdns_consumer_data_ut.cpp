#include <random>
#include <algorithm>
#include <memory>
#include <gtest/gtest.h>
#include <QtCore>
#include <plugins/resource/mdns/mdns_consumer_data.h>
#include <nx/utils/random.h>


namespace nx {
namespace test {

class TestDataGenerator
{
public:
    TestDataGenerator(
        int consumersCount,
        int remoteAddressCount,
        int entriesPerRemoteAddressCount)
        :
        m_gen(m_rd())
    {
        generateConsumers(consumersCount);
        generateRemoteAddresses(remoteAddressCount);
        generateLocalAddresses();
        generateResponses(entriesPerRemoteAddressCount);
    }

    bool visitEntry(
        const QString& remoteAddress,
        const QString& localAddress,
        const QByteArray& response)
    {
        if (!m_remoteAddresses.contains(remoteAddress)
            || !m_localAddresses.contains(localAddress)
            || !m_responses.contains(response))
        {
            return false;
        }

        m_visited++;
        return true;
    }

    int visitedCount() const
    {
        return m_visited;
    }

    void resetVisited()
    {
        m_visited = 0;
    }

    template<typename Visitor>
    void forEachEntry(Visitor visitor) const
    {
        for (const auto& remoteAddr: m_remoteAddresses)
            visitResponsesWithRemoteAddr(visitor, remoteAddr);
    }

    uint64_t unusedId() const
    {
        for (uint64_t i = 0; i < std::numeric_limits<uint64_t>::max(); ++i)
        {
            if (!m_ids.contains(i))
                return i;
        }

        NX_ASSERT(false);
        return -1ULL;
    }

    QSet<uint64_t> consumers() const { return m_ids; }

private:
    static const int kLocalAddressesCount;

    QSet<uint64_t> m_ids;
    QSet<QString> m_remoteAddresses;
    QSet<QString> m_localAddresses;
    QSet<QByteArray> m_responses;
    int m_visited = 0;
    // Until nx::utils::random::* work well
    mutable std::random_device m_rd;
    mutable std::mt19937 m_gen;


    void generateConsumers(int consumersCount)
    {
        while (m_ids.size() < consumersCount)
        {
            uint64_t id = nx::utils::random::number(
                (uint64_t) 0,
                std::numeric_limits<uint64_t>::max());

            if (m_ids.contains(id))
                continue;

            m_ids.insert(id);
        }
    }

    void generateRemoteAddresses(int addressCount)
    {
        generateDataSet(
            [this] (const QByteArray& data)
            {
                return m_remoteAddresses.contains(data);
            },
            m_remoteAddresses,
            addressCount);
    }

    void generateLocalAddresses()
    {
        generateDataSet(
            [this] (const QByteArray& data)
            {
                return m_localAddresses.contains(data);
            },
            m_localAddresses,
            kLocalAddressesCount);
    }

    void generateResponses(int entriesPerRemoteAddressCount)
    {
        generateDataSet(
            [this] (const QByteArray& data)
            {
                return m_responses.contains(data);
            },
            m_responses,
            entriesPerRemoteAddressCount);
    }

    QByteArray generateRandomData(int size)
    {
        QByteArray result;
        std::uniform_int_distribution<int> dis(65, 122);

        try
        {
            for (int i = 0; i < size; ++i)
                result.push_back(dis(m_gen));
        }
        catch(...)
        {
            result = nx::utils::random::generate(size);
        }

        return result;
    }

    template<typename CheckExistencePred>
    QByteArray generateUniqueData(CheckExistencePred pred, int size = 64)
    {
        auto result = generateRandomData(size);
        while (pred(result))
            result = generateRandomData(size);

        return result;
    }

    template<typename CheckExistencePred, typename SetType>
    void generateDataSet(CheckExistencePred pred, SetType& set, int count)
    {
        while (set.size() < count)
        {
            auto entry = generateUniqueData(pred);
            set.insert(entry);
        }
    }

    template<typename Visitor>
    void visitResponsesWithRemoteAddr(Visitor visitor, const QString& remoteAddr) const
    {
        for (const auto& response: m_responses)
            visitor(remoteAddr, chooseAnyLocalAddr(), response);
    }

    QString chooseAnyLocalAddr() const
    {
        int addrIndex;
        try
        {
            std::uniform_int_distribution<int> dis(0, m_localAddresses.size() - 1);
            addrIndex = dis(m_gen);
        }
        catch (...)
        {
            addrIndex = nx::utils::random::number(0, m_localAddresses.size() - 1);
        }

        int cur = 0;
        for (const auto& localAddr: m_localAddresses)
        {
            if (cur++ == addrIndex)
                return localAddr;
        }

        NX_ASSERT(false);
        return QString();
    }
};

const int TestDataGenerator::kLocalAddressesCount = 3;


class ConsumerDataList: public ::testing::Test
{
protected:
    ConsumerDataList():
        m_generator(
            std::unique_ptr<TestDataGenerator>(
                new TestDataGenerator(
                    kConsumersCount,
                    kRemoteAddressCount,
                    kEntriesCount)))
    {

    }

    void whenSomeConsumersHaveBeenRegistered()
    {
        for (const auto& id: m_generator->consumers())
        {
            m_dataList.registerConsumer(id);
            // no duplicates are allowed
            ASSERT_FALSE(m_dataList.registerConsumer(id));
        }
    }

    void whenTestDataHasBeenAdded()
    {
        m_generator->forEachEntry(
            [this](const QString& remoteAddr, const QString& localAddr, const QByteArray& response)
            {
                m_dataList.addData(remoteAddr, localAddr, response);
            });
    }

    void whenExceedPacketsAccumulatesInTheListener()
    {
        m_generator = std::unique_ptr<TestDataGenerator>(
            new TestDataGenerator(
                kConsumersCount,
                kRemoteAddressCount,
                kExceedEntriesCount));

        whenSomeConsumersHaveBeenRegistered();
        whenTestDataHasBeenAdded();
    }

    void andWhenDataListHasBeelClearedOfReadData()
    {
        m_dataList.clearRead();
    }

    void thenDataForAllOfThemShouldBeFound()
    {
        for (const auto& id: m_generator->consumers())
            ASSERT_NE(m_dataList.data(id), nullptr);
    }

    void andNoDataForOthersShouldBeFound()
    {
        ASSERT_EQ(m_dataList.data(m_generator->unusedId()), nullptr);
    }

    void thenAllDataShouldExistInList()
    {
        for (auto id: m_generator->consumers())
        {
            verifyData(m_dataList.data(id), kRemoteAddressCount * kEntriesCount);
            m_generator->resetVisited();
        }
    }

    void thenEveryConsumerShouldBecomeEmpty()
    {
        for (auto id: m_generator->consumers())
            verifyData(m_dataList.data(id), 0);
    }

    void thenOnlyNonExceededDataShouldExist()
    {
        for (auto id: m_generator->consumers())
        {
            verifyData(m_dataList.data(id), kRemoteAddressCount * ConsumerData::kMaxEntriesCount);
            m_generator->resetVisited();
        }
    }

private:
    const static int kConsumersCount = 2;
    const static int kRemoteAddressCount = 5;
    const static int kEntriesCount = 100;
    const static int kExceedEntriesCount = 9000;

    detail::ConsumerDataList m_dataList;
    std::unique_ptr<TestDataGenerator> m_generator;


    void verifyData(const ConsumerData* consumerData, int expectedCount)
    {
        consumerData->forEachEntry(
            [this](
                const QString& remoteAddr,
                const QString& localAddr,
                const QByteArray& response)
            {
                ASSERT_TRUE(m_generator->visitEntry(remoteAddr, localAddr, response));
            });
        ASSERT_EQ(expectedCount, m_generator->visitedCount());
    }
};


TEST_F(ConsumerDataList, registerConsumers)
{
    whenSomeConsumersHaveBeenRegistered();
    thenDataForAllOfThemShouldBeFound();
    andNoDataForOthersShouldBeFound();
}

TEST_F(ConsumerDataList, addData)
{
    whenSomeConsumersHaveBeenRegistered();
    whenTestDataHasBeenAdded();
    thenAllDataShouldExistInList();

    andWhenDataListHasBeelClearedOfReadData();
    thenEveryConsumerShouldBecomeEmpty();
}

TEST_F(ConsumerDataList, addData_Overflow)
{
    whenExceedPacketsAccumulatesInTheListener();
    thenOnlyNonExceededDataShouldExist();
}


} // namespace test
} // namespace nx
