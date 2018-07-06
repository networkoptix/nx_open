#include <gtest/gtest.h>
#include <utils/memory/cyclic_allocator.h>
#include <utils/memory/system_allocator.h>
#include <utils/common/byte_array.h>

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
#include <nx/utils/thread/long_runnable.h>

namespace nx {
namespace utils {
namespace memory {
namespace test {

// CyclicAllocatorUser -----------------------------------------------------------------------------
struct CyclicAllocatorUserSettings
{
    size_t dataSize = 1 * 1024 * 1024;
    size_t maxSize = 3 * 1024 * 1024;
    size_t step = 200 * 1024;
    size_t maxQueueSize = 16;
    std::chrono::milliseconds sleepTimeout{1};
};

class CyclicAllocatorUser: public QnLongRunnable
{
public:
    CyclicAllocatorUser(const CyclicAllocatorUserSettings& settings);
    virtual ~CyclicAllocatorUser() override;
    virtual void run() override;
private:
    CyclicAllocator m_allocator;
    std::deque<std::shared_ptr<QnByteArray>> m_packets;
    std::vector<char> m_testData;
    CyclicAllocatorUserSettings m_settings;

    void resizeDataIfNeeded();
    void popExcessPackets();
};

CyclicAllocatorUser::CyclicAllocatorUser(const CyclicAllocatorUserSettings &settings):
    m_settings(settings)
{
    m_testData.resize(settings.dataSize);
}

CyclicAllocatorUser::~CyclicAllocatorUser()
{
    stop();
}

void CyclicAllocatorUser::run()
{
    while (!m_needStop)
    {
        auto newPacket = std::shared_ptr<QnByteArray>(new QnByteArray(&m_allocator, 32, 0));
        newPacket->write(m_testData.data(), m_testData.size());
        m_packets.push_back(newPacket);

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

// -------------------------------------------------------------------------------------------------

// ProcessRamUsageFetcher --------------------------------------------------------------------------
class ProcessRamUsageFetcher
{
public:
    int64_t get();
private:
    std::unique_ptr<std::ifstream> m_file;
    int64_t m_value = -1;

    bool reset();
    bool processFileLine();
    bool rssFound() const;
    void parseLine(const std::string& line);

    static std::string toLower(const std::string& line);
};

int64_t ProcessRamUsageFetcher::get()
{
    if (!reset())
        return -1;

    while (processFileLine() && !rssFound())
        ;

    return m_value;
}

bool ProcessRamUsageFetcher::reset()
{
    m_value = -1;
    m_file.reset(new std::ifstream("/proc/self/status"));
    if (!m_file || !m_file->is_open())
        return false;

    return true;
}

bool ProcessRamUsageFetcher::processFileLine()
{
    if (m_file->fail() || m_file->eof())
        return false;

    std::string line;
    if (!std::getline(*m_file, line))
        return false;

    parseLine(toLower(line));
    return true;
}

std::string ProcessRamUsageFetcher::toLower(const std::string& line)
{
    std::string result;
    std::transform(
        line.cbegin(),
        line.cend(),
        std::back_inserter(result),
        [](char c)
        {
            return std::tolower(c);
        });

    return result;
}

void ProcessRamUsageFetcher::parseLine(const std::string& line)
{
    if (line.find("vmrss") == std::string::npos)
        return;

    std::stringstream parseStream(line);
    std::string name;
    std::string measure;

    parseStream >> name >> m_value >> measure;
    if (m_value == -1)
        return;

    if (measure == "kb")
        m_value *= 1024;

    if (measure == "mb")
        m_value *= 1024 * 1024;
}

bool ProcessRamUsageFetcher::rssFound() const
{
    return m_value != -1;
}

static int64_t processMemoryUsed()
{
    return ProcessRamUsageFetcher().get();
}
// -------------------------------------------------------------------------------------------------

// Test class --------------------------------------------------------------------------------------
class CyclicAllocator: public ::testing::Test
{
protected:
    virtual void TearDown() override;

    void givenAllocatorUserWithDefaultSettings();
    void whenAllocatorWorksForSomeTime();
    void thenMemoryConsumedShouldHaveNotConsiderablyGrown();

private:
    int64_t m_firstMemoryProbe;
    int64_t m_secondMemoryProbe;
    std::unique_ptr<CyclicAllocatorUser> m_allocatorUser;

    void workAndMeasure(int64_t* outValue);
};

void CyclicAllocator::TearDown()
{
    m_allocatorUser.reset();
}

void CyclicAllocator::givenAllocatorUserWithDefaultSettings()
{
    m_allocatorUser.reset(new CyclicAllocatorUser(CyclicAllocatorUserSettings()));
}

void CyclicAllocator::whenAllocatorWorksForSomeTime()
{
    m_allocatorUser->start();
    workAndMeasure(&m_firstMemoryProbe);
    workAndMeasure(&m_secondMemoryProbe);
}

void CyclicAllocator::workAndMeasure(int64_t* outValue)
{
    std::this_thread::sleep_for(std::chrono::seconds(4));
    *outValue = processMemoryUsed();
    ASSERT_NE(-1, *outValue);
}

void CyclicAllocator::thenMemoryConsumedShouldHaveNotConsiderablyGrown()
{
    ASSERT_LT(std::abs(m_secondMemoryProbe - m_firstMemoryProbe), 100 * 1024);
}
// -------------------------------------------------------------------------------------------------

#if defined (Q_OS_LINUX)

TEST_F(CyclicAllocator, ConstantMemoryConsuming)
{
    givenAllocatorUserWithDefaultSettings();
    whenAllocatorWorksForSomeTime();
    thenMemoryConsumedShouldHaveNotConsiderablyGrown();
}

#endif

} // namespace test
} // namespace memory
} // namespace utils
} // namespace nx

