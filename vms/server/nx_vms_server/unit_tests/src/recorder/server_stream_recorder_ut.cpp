#include <recorder/server_stream_recorder.h>
#include <gtest/gtest.h>

#include <nx/mediaserver/camera_mock.h>

namespace nx::vms::server::test {

namespace {

class TestData: public QnAbstractMediaData
{
public:
    TestData(size_t size):
        QnAbstractMediaData(QnAbstractMediaData::DataType::VIDEO),
        m_size(size)
    {
    }

    QnAbstractMediaData* clone() const override {  return nullptr; };
    const char* data() const override { return nullptr; }
    size_t dataSize() const override { return m_size; }

private:
    const size_t m_size = 0;
};

class TestRecorder: public QnServerStreamRecorder
{
public:
    using QnServerStreamRecorder::QnServerStreamRecorder;
    using QnServerStreamRecorder::putData;
    using QnServerStreamRecorder::runCycle;
    using QnServerStreamRecorder::endOfRun;

    void beforeProcessData(const QnConstAbstractMediaDataPtr&) override {}
    bool isRunning() const override { return true; }
};

class ServerStreamRecorderTest: public resource::test::CameraTest
{
public:
    std::unique_ptr<TestRecorder> newRecorder()
    {
        return std::make_unique<TestRecorder>(
            serverModule(), newCamera(), QnServer::HiQualityCatalog, nullptr);
    }
};

} // namespace

TEST_F(ServerStreamRecorderTest, totalQueued)
{
    const auto recorder = newRecorder();
    EXPECT_EQ(QnServerStreamRecorder::totalQueuedPackets(), 0);
    EXPECT_EQ(QnServerStreamRecorder::totalQueuedBytes(), 0);

    recorder->putData(std::make_shared<TestData>(100));
    EXPECT_EQ(QnServerStreamRecorder::totalQueuedPackets(), 1);
    EXPECT_EQ(QnServerStreamRecorder::totalQueuedBytes(), 100);

    recorder->putData(std::make_shared<TestData>(200));
    EXPECT_EQ(QnServerStreamRecorder::totalQueuedPackets(), 2);
    EXPECT_EQ(QnServerStreamRecorder::totalQueuedBytes(), 300);

    recorder->runCycle();
    EXPECT_EQ(QnServerStreamRecorder::totalQueuedPackets(), 1);
    EXPECT_EQ(QnServerStreamRecorder::totalQueuedBytes(), 200);

    recorder->endOfRun();
    EXPECT_EQ(QnServerStreamRecorder::totalQueuedPackets(), 0);
    EXPECT_EQ(QnServerStreamRecorder::totalQueuedBytes(), 0);
}

} // namespace nx::vms::server::test
