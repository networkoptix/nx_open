#include <gtest/gtest.h>

#include <memory>
#include <deque>
#include <vector>
#include <thread>
#include <chrono>
#include <iostream>
#include <cstdint>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <cctype>

#include <nx/utils/memory/cyclic_allocator.h>
#include <nx/utils/memory/system_allocator.h>
#include <nx/utils/thread/long_runnable.h>

namespace nx::utils::memory::test {

struct CyclicAllocatorUserSettings
{
    size_t dataSize = 1 * 1024 * 1024;
    size_t maxSize = 3 * 1024 * 1024;
    size_t step = 200 * 1024;
    size_t maxQueueSize = 16;
    std::chrono::milliseconds sleepTimeout{1};
};

class SampleDynamicArray
{
public:
    SampleDynamicArray(QnAbstractAllocator* allocator, std::size_t capacity):
        m_allocator(allocator)
    {
        if (capacity)
            reserve(capacity);
    }

    ~SampleDynamicArray()
    {
        if (m_data)
            m_allocator->release(m_data);
    }

    SampleDynamicArray(const SampleDynamicArray&) = delete;
    SampleDynamicArray& operator=(const SampleDynamicArray&) = delete;

    void append(const char* data, std::size_t count)
    {
        if (m_size + count > m_capacity)
            reserve(m_size + count);

        memcpy(m_data + m_size, data, count);
        m_size += count;
    }

    void reserve(std::size_t capacity)
    {
        if (capacity <= m_capacity)
            return;

        char* newData = (char*) m_allocator->alloc(capacity);
        memcpy(newData, m_data, m_size);

        m_allocator->release(m_data);
        m_data = newData;
        m_capacity = capacity;
    }

private:
    QnAbstractAllocator* m_allocator = nullptr;
    char* m_data = nullptr;
    std::size_t m_size = 0;
    std::size_t m_capacity = 0;
};

//-------------------------------------------------------------------------------------------------

class CyclicAllocatorUser:
    public QnLongRunnable
{
public:
    CyclicAllocatorUser(
        const CyclicAllocatorUserSettings& settings,
        CyclicAllocator* allocator);

    virtual ~CyclicAllocatorUser() override;

    void waitUntilMaxMemoryIsAllocated();

protected:
    virtual void run() override;

private:
    CyclicAllocatorUserSettings m_settings;
    CyclicAllocator* m_allocator = nullptr;
    std::deque<std::shared_ptr<SampleDynamicArray>> m_packets;
    std::vector<char> m_testData;
    std::atomic<std::size_t> m_totalPacketsAllocated{0};

    void resizeDataIfNeeded();
    void popExcessPackets();
};

CyclicAllocatorUser::CyclicAllocatorUser(
    const CyclicAllocatorUserSettings& settings,
    CyclicAllocator* allocator)
    :
    m_settings(settings),
    m_allocator(allocator)
{
    m_testData.resize(settings.dataSize);
}

CyclicAllocatorUser::~CyclicAllocatorUser()
{
    stop();
}

void CyclicAllocatorUser::waitUntilMaxMemoryIsAllocated()
{
    while (m_totalPacketsAllocated < m_settings.maxQueueSize)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

void CyclicAllocatorUser::run()
{
    while (!m_needStop)
    {
        auto newPacket = std::make_shared<SampleDynamicArray>(m_allocator, 0);
        newPacket->append(m_testData.data(), m_testData.size());
        m_packets.push_back(newPacket);
        ++m_totalPacketsAllocated;

        resizeDataIfNeeded();
        popExcessPackets();

        std::this_thread::sleep_for(m_settings.sleepTimeout);
    }
}

void CyclicAllocatorUser::resizeDataIfNeeded()
{
    if (m_settings.dataSize >= m_settings.maxSize)
        return;

    m_settings.dataSize += m_settings.step;
    m_testData.resize(m_settings.dataSize);
}

void CyclicAllocatorUser::popExcessPackets()
{
    while (m_packets.size() > m_settings.maxQueueSize)
        m_packets.pop_front();
}

//-------------------------------------------------------------------------------------------------

class CyclicAllocator:
    public ::testing::Test
{
protected:
    virtual void TearDown() override
    {
        m_allocatorUser.reset();
    }

    void givenAllocatorUserWithDefaultSettings()
    {
        m_allocatorUser = std::make_unique<CyclicAllocatorUser>(
            CyclicAllocatorUserSettings(), &m_allocator);
    }

    void whenAllocatorWorksForSomeTime()
    {
        m_allocatorUser->start();
        m_allocatorUser->waitUntilMaxMemoryIsAllocated();

        workAndMeasure(&m_firstMemoryProbe);
        workAndMeasure(&m_secondMemoryProbe);
    }

    void thenAllocatorMemoryUsageStaysConstant()
    {
        ASSERT_EQ(m_firstMemoryProbe, m_secondMemoryProbe);
    }

private:
    int64_t m_firstMemoryProbe = 0;
    int64_t m_secondMemoryProbe = 0;
    ::CyclicAllocator m_allocator;
    std::unique_ptr<CyclicAllocatorUser> m_allocatorUser;

    void workAndMeasure(int64_t* outValue)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        *outValue = m_allocator.totalBytesAllocated();
        ASSERT_NE(-1, *outValue);
    }
};

//-------------------------------------------------------------------------------------------------

TEST_F(CyclicAllocator, constant_memory_consumption)
{
    givenAllocatorUserWithDefaultSettings();
    whenAllocatorWorksForSomeTime();
    thenAllocatorMemoryUsageStaysConstant();
}

} // namespace nx::utils::memory::test
