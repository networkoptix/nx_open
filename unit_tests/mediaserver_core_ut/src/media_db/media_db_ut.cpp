#define GTEST_HAS_TR1_TUPLE     0
#define GTEST_USE_OWN_TR1_TUPLE 1

#define GTEST_HAS_POSIX_RE 0
#include <gtest/gtest.h>

#include <random>
#include <climits>
#include <vector>
#include <algorithm>

#include "utils/media_db/media_db.h"
#include "common/common_globals.h"
#include <QtCore>

bool recursiveClean(const QString &path);

template<qint64 From, qint64 To>
qint64 genRandomNumber()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<qint64> dist(From, To);

    return dist(gen);
}

void generateCameraUid(QByteArray *camUid, size_t n)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<int> dist(65, 90);

    for (size_t i = 0; i < n; i++)
        camUid->append(dist(gen));
}

struct TestFileHeader
{
    int dbVersion;
};

TestFileHeader generateFileHeader()
{
    return { (int)genRandomNumber<0, 127>() };
}

struct TestFileOperation
{
    qint64 startTime;
    qint64 fileSize;
    int code;
    int cameraId;
    int duration;
    int chunksCatalog;
};

bool operator == (const TestFileOperation &lhs, const TestFileOperation &rhs)
{
    return lhs.startTime == rhs.startTime && lhs.fileSize == rhs.fileSize &&
           lhs.code == rhs.code && lhs.cameraId == rhs.cameraId &&
           lhs.duration == rhs.duration && lhs.chunksCatalog == rhs.chunksCatalog;
}

TestFileOperation generateFileOperation(int code)
{
    return{ genRandomNumber<0, 4398046511103LL>(), genRandomNumber<0, 140737488355327LL>(),
            code, (int)genRandomNumber<0, 65535>(),
            (int)genRandomNumber<0, 1048575LL>(), (int)genRandomNumber<0, 1>() };
}

struct TestCameraOperation
{
    int code;
    int uuidLen;
    int id;
    QByteArray camUniqueId;
};

bool operator == (const TestCameraOperation &lhs, const TestCameraOperation &rhs)
{
    return lhs.code == rhs.code && lhs.uuidLen == rhs.uuidLen &&
           lhs.id == rhs.id && lhs.camUniqueId == rhs.camUniqueId;
}

TestCameraOperation generateCameraOperation()
{
    int camUidLen = (int)genRandomNumber<20, 40>();
    QByteArray camUid;
    generateCameraUid(&camUid, camUidLen);

    return{ 2, camUidLen, (int)genRandomNumber<0, 65535>(), camUid };
}

typedef boost::variant<TestFileHeader, TestFileOperation, TestCameraOperation> TestRecord;

class RecordWriteVisitor : public boost::static_visitor<>
{
public:
    RecordWriteVisitor(nx::media_db::DbHelper *helper) : m_helper(helper) {}

    void operator() (const TestFileHeader &tfh) const
    {
        m_helper->writeFileHeader(tfh.dbVersion);
    }

    void operator() (const TestFileOperation &tfo) const
    {
        nx::media_db::MediaFileOperation fileOp;
        fileOp.setCameraId(tfo.cameraId);
        fileOp.setCatalog(tfo.chunksCatalog);
        fileOp.setDuration(tfo.duration);
        fileOp.setFileSize(tfo.fileSize);
        fileOp.setRecordType(nx::media_db::RecordType(tfo.code));
        fileOp.setStartTime(tfo.startTime);

        m_helper->writeRecordAsync(fileOp);
    }

    void operator() (const TestCameraOperation &tco) const
    {
        nx::media_db::CameraOperation camOp;
        camOp.setCameraId(tco.id);
        camOp.setCameraUniqueIdLen(tco.uuidLen);
        camOp.setRecordType(nx::media_db::RecordType(tco.code));
        camOp.setCameraUniqueId(tco.camUniqueId);

        m_helper->writeRecordAsync(camOp);
    }
private:
    nx::media_db::DbHelper *m_helper;
};

struct TestData
{
    TestRecord data;
    bool visited;

    TestData(const TestRecord &record, bool visited) : data(record), visited(visited) {}
};
typedef std::vector<TestData> TestDataVector;

struct TestDataManager
{
    TestDataManager(size_t dataSize)
    {
        dataVector.emplace_back(generateFileHeader(), false);
        for (size_t i = 0; i < dataSize; ++i) 
        {
            switch (genRandomNumber<0, 1>()) 
            {
            case 0:
                switch (genRandomNumber<0, 1>()) 
                {
                case 0:
                    dataVector.emplace_back(generateFileOperation(0), false);
                    break;
                case 1:
                    dataVector.emplace_back(generateFileOperation(1), false);
                    break;
                }
                break;
            case 1:
                dataVector.emplace_back(generateCameraOperation(), false);
                break;
            }
        }
    }

    template<typename Record>
    bool seekAndSet(const Record &record)
    {
        auto it = std::find_if(dataVector.begin(), dataVector.end(),
                               [&record](const TestData &td) 
                               {
                                   if (const Record* r = boost::get<Record>(&td.data))
                                       return *r == record && !td.visited;
                                   return false;
                               });
        if (it != dataVector.end())
        {
            it->visited = true;
            return true;
        }
        return false;
    }

    TestDataVector dataVector;
};

class TestDbHelperHandler : public nx::media_db::DbHelperHandler
{
public:
    TestDbHelperHandler(nx::media_db::Error *error, TestDataManager *tdm) 
        : m_error(error),
          m_tdm(tdm)
    {}

    void handleCameraOp(const nx::media_db::CameraOperation &cameraOp, 
                        nx::media_db::Error error) override
    {
        if ((*m_error = error) == nx::media_db::Error::ReadError)
            return;

        TestCameraOperation top;
        top.camUniqueId = cameraOp.getCameraUniqueId();
        top.code = (int)cameraOp.getRecordType();
        top.id = cameraOp.getCameraId();
        top.uuidLen = cameraOp.getCameraUniqueIdLen();
        
        ASSERT_TRUE(m_tdm->seekAndSet(top));
    }

    void handleMediaFileOp(const nx::media_db::MediaFileOperation &mediaFileOp, 
                           nx::media_db::Error error) override
    {
        if ((*m_error = error) == nx::media_db::Error::ReadError)
            return;

        TestFileOperation tfop;
        tfop.cameraId = mediaFileOp.getCameraId();
        tfop.chunksCatalog = mediaFileOp.getCatalog();
        tfop.code = (int)mediaFileOp.getRecordType();
        tfop.duration = mediaFileOp.getDuration();
        tfop.fileSize = mediaFileOp.getFileSize();
        tfop.startTime = mediaFileOp.getStartTime();

        ASSERT_TRUE(m_tdm->seekAndSet(tfop));
    }

    void handleError(nx::media_db::Error error) override
    {
        *m_error = error;
    }

    void handleRecordWrite(nx::media_db::Error error) override
    {
        *m_error = error;
        ASSERT_TRUE(error == nx::media_db::Error::NoError ||
                    error == nx::media_db::Error::Eof);
    }
private:
    nx::media_db::Error *m_error;
    TestDataManager *m_tdm;
};


void initDbFile(QFile *dbFile)
{
    const QString workDirPath = qApp->applicationDirPath() + lit("/tmp/media_db");
    QDir().mkpath(workDirPath);

    dbFile->setFileName(workDirPath + lit("/file.mdb"));
    dbFile->open(QIODevice::ReadWrite);
}

TEST(MediaDb_test, BitsTwiddling)
{
    const size_t kTestBufSize = 10000;
    std::vector<TestFileHeader> fhVector;

    for (size_t i = 0; i < kTestBufSize; ++i)
        fhVector.push_back(generateFileHeader());

    for (size_t i = 0; i < kTestBufSize; ++i)
    {
        nx::media_db::FileHeader fh;
        fh.setDbVersion(fhVector[i].dbVersion);
        ASSERT_TRUE(fh.getDbVersion() == fhVector[i].dbVersion);
    }

    std::vector<TestFileOperation> fopVector;

    for (size_t i = 0; i < kTestBufSize;  ++i)
        fopVector.push_back(generateFileOperation(i % 2));

    for (size_t i = 0; i < kTestBufSize; ++i)
    {
        nx::media_db::MediaFileOperation mfop;
        const TestFileOperation &tfop = fopVector[i];

        mfop.setCameraId(tfop.cameraId);
        mfop.setCatalog(tfop.chunksCatalog);
        mfop.setDuration(tfop.duration);
        mfop.setFileSize(tfop.fileSize);
        mfop.setRecordType(nx::media_db::RecordType(tfop.code));

        ASSERT_TRUE(mfop.getCameraId() == tfop.cameraId);
        ASSERT_TRUE(mfop.getCatalog() == tfop.chunksCatalog);
        ASSERT_TRUE(mfop.getDuration() == tfop.duration);
        ASSERT_TRUE(mfop.getFileSize() == tfop.fileSize);
        ASSERT_TRUE(mfop.getRecordType() == nx::media_db::RecordType(tfop.code));
    }

    std::vector<TestCameraOperation> copVector;

    for (size_t i = 0; i < kTestBufSize;  ++i)
        copVector.push_back(generateCameraOperation());

    for (size_t i = 0; i < kTestBufSize; ++i)
    {
        nx::media_db::CameraOperation cop;
        const TestCameraOperation &tcop = copVector[i];

        cop.setCameraId(tcop.id);
        cop.setCameraUniqueId(tcop.camUniqueId);
        cop.setCameraUniqueIdLen(tcop.uuidLen);
        cop.setRecordType(nx::media_db::RecordType(tcop.code));

        ASSERT_TRUE(cop.getCameraId() == tcop.id);
        ASSERT_TRUE(cop.getCameraUniqueId() == tcop.camUniqueId);
        ASSERT_TRUE(cop.getCameraUniqueIdLen() == tcop.uuidLen);
        ASSERT_TRUE(cop.getRecordType() == nx::media_db::RecordType(tcop.code));
    }
}

TEST(MediaDb_test, ReadWrite)
{
    const QString workDirPath = qApp->applicationDirPath() + lit("/tmp/media_db");
    QFile dbFile;
    initDbFile(&dbFile);

    nx::media_db::Error error;
    TestDataManager tdm(10000);
    TestDbHelperHandler testHandler(&error, &tdm);
    nx::media_db::DbHelper dbHelper(&dbFile, &testHandler);

    // Wrong mode test 
    error = dbHelper.writeFileHeader(
        boost::get<TestFileHeader>(
            tdm.dataVector[0].data).dbVersion);

    ASSERT_TRUE(error == nx::media_db::Error::WrongMode);

    dbHelper.setMode(nx::media_db::Mode::Write);

    for (auto &data : tdm.dataVector)
        boost::apply_visitor(RecordWriteVisitor(&dbHelper), data.data);

    dbHelper.reset();
    dbFile.close();
    dbFile.open(QIODevice::ReadWrite);

    uint8_t dbVersion;
    error = dbHelper.readFileHeader(&dbVersion);
    ASSERT_TRUE(error == nx::media_db::Error::WrongMode);

    dbHelper.setMode(nx::media_db::Mode::Read);
    error = dbHelper.readFileHeader(&dbVersion);

    ASSERT_TRUE(dbVersion == boost::get<TestFileHeader>(tdm.dataVector[0].data).dbVersion);
    tdm.dataVector[0].visited = true;
    ASSERT_TRUE(error == nx::media_db::Error::NoError);

    while (error == nx::media_db::Error::NoError)
        dbHelper.readRecord();

    dbFile.close();
    recursiveClean(workDirPath);

    size_t readRecords = std::count_if(tdm.dataVector.cbegin(), tdm.dataVector.cend(),
                                       [](const TestData &td) { return td.visited; });
    ASSERT_TRUE(readRecords == tdm.dataVector.size());
}

TEST(MediaDb_test, DbFileTruncate)
{
    const QString workDirPath = qApp->applicationDirPath() + lit("/tmp/media_db");
    int truncateCount = 1000;

    while (truncateCount-- >= 0)
    {
        QFile dbFile;
        initDbFile(&dbFile);

        nx::media_db::Error error;
        TestDataManager tdm(1);
        TestDbHelperHandler testHandler(&error, &tdm);
        nx::media_db::DbHelper dbHelper(&dbFile, &testHandler);
        dbHelper.setMode(nx::media_db::Mode::Write);

        for (auto &data : tdm.dataVector)
            boost::apply_visitor(RecordWriteVisitor(&dbHelper), data.data);

        dbHelper.setMode(nx::media_db::Mode::Read);
        dbFile.close();
        dbFile.open(QIODevice::ReadOnly);

        QByteArray content = dbFile.readAll();
        ASSERT_TRUE(dbFile.remove());
        // truncating randomly last record
        content.truncate(content.size() - genRandomNumber<1, sizeof(qint64) * 2 - 1>());

        initDbFile(&dbFile);
        dbFile.write(content);
        dbFile.close();

        dbFile.open(QIODevice::ReadOnly);
        dbHelper.setDevice(&dbFile);

        uint8_t dbVersion;
        error = dbHelper.readFileHeader(&dbVersion);

        ASSERT_TRUE(dbVersion == boost::get<TestFileHeader>(tdm.dataVector[0].data).dbVersion);
        tdm.dataVector[0].visited = true;
        ASSERT_TRUE(error == nx::media_db::Error::NoError) << (int)error;

        while (error == nx::media_db::Error::NoError)
            dbHelper.readRecord();

        ASSERT_TRUE(error == nx::media_db::Error::ReadError) << (int)error;
        ASSERT_TRUE(dbFile.remove());

        size_t readRecords = std::count_if(tdm.dataVector.cbegin(), tdm.dataVector.cend(),
                                           [](const TestData &td) { return td.visited; });
        // we've read all except the very last one record
        ASSERT_TRUE(readRecords == tdm.dataVector.size() - 1) << readRecords;
    }
}

TEST(MediaDb_test, ReadWrite_MT)
{
    const QString workDirPath = qApp->applicationDirPath() + lit("/tmp/media_db");
    QFile dbFile;
    initDbFile(&dbFile);

    const size_t threadsNum = 4;
    nx::media_db::Error error;
    TestDataManager tdm(threadsNum * 1000);
    TestDbHelperHandler testHandler(&error, &tdm);
    nx::media_db::DbHelper dbHelper(&dbFile, &testHandler);
    dbHelper.setMode(nx::media_db::Mode::Write);

    //write header explicitely
    boost::apply_visitor(RecordWriteVisitor(&dbHelper), tdm.dataVector[0].data);

    std::vector<std::thread> threads;
    for (size_t i = 0; i < threadsNum; ++i)
    {
            threads.emplace_back(
                std::thread([&dbHelper, &tdm, i]
                            {
                                size_t j = i == 0 ? 1 : i * 1000;
                                for (; j < (i + 1) * 1000; ++j)
                                    boost::apply_visitor(RecordWriteVisitor(&dbHelper),
                                                         tdm.dataVector[j].data);
                            }));
    }

    for (size_t i = 0; i < threadsNum; ++i)
        if (threads[i].joinable())
            threads[i].join();

    dbHelper.setMode(nx::media_db::Mode::Read);
    dbHelper.reset();
    dbFile.close();
    dbFile.open(QIODevice::ReadWrite);

    uint8_t dbVersion;
    error = dbHelper.readFileHeader(&dbVersion);

    ASSERT_TRUE(dbVersion == boost::get<TestFileHeader>(tdm.dataVector[0].data).dbVersion);
    tdm.dataVector[0].visited = true;
    ASSERT_TRUE(error == nx::media_db::Error::NoError);

    while (error == nx::media_db::Error::NoError)
        dbHelper.readRecord();

    dbFile.close();
    recursiveClean(workDirPath);

    size_t readRecords = std::count_if(tdm.dataVector.cbegin(), tdm.dataVector.cend(),
                                       [](const TestData &td) { return td.visited; });
    ASSERT_TRUE(readRecords + 1 == tdm.dataVector.size());
}

TEST(MediaDb_test, Cleanup)
{
    const QString workDirPath = qApp->applicationDirPath() + lit("/tmp/media_db");
    recursiveClean(workDirPath);
}