#define GTEST_HAS_TR1_TUPLE     0
#define GTEST_USE_OWN_TR1_TUPLE 1

#define GTEST_HAS_POSIX_RE 0
#include <gtest/gtest.h>

#include <climits>
#include <vector>
#include <set>
#include <algorithm>
#include <cerrno>
#include <sstream>

#include <QtCore>
#include <QtSql>

#include <utils/common/writer_pool.h>
#include "plugins/storage/file_storage/file_storage_resource.h"
#include <recorder/storage_manager.h>
#include <core/resource_management/status_dictionary.h>
#include <common/common_module.h>
#include <core/resource_management/resource_properties.h>
#include <utils/common/util.h>
#include <utils/db/db_helper.h>
#include <nx/utils/random.h>

#ifdef Q_OS_LINUX
#   include <platform/monitoring/global_monitor.h>
#   include <platform/platform_abstraction.h>
#endif

#include <test_support/utils.h>
#include <media_server/settings.h>
#include <media_server/media_server_module.h>

#include "../media_server_module_fixture.h"


namespace nx::media_db::test {

class MediaDbWriteRead: public ::testing::Test
{
protected:
private:
    MediaDbReader m_reader;
};

} // namespace nx::media_db::test

void generateCameraUid(QByteArray *camUid, size_t n)
{
    for (size_t i = 0; i < n; i++)
        camUid->append(nx::utils::random::number(65, 90));
}

struct TestFileHeader
{
    int dbVersion;
};

TestFileHeader generateFileHeader()
{
    return{ nx::utils::random::number(0, 127) };
}

struct TestFileOperation
{
    qint64 startTime;
    qint64 fileSize;
    int code;
    int cameraId;
    int duration;
    int timeZone;
    int chunksCatalog;
    int fileIndex;
};

bool operator == (const TestFileOperation &lhs, const TestFileOperation &rhs)
{
    return lhs.startTime == rhs.startTime && lhs.fileSize == rhs.fileSize &&
        lhs.code == rhs.code && lhs.cameraId == rhs.cameraId &&
        lhs.timeZone == rhs.timeZone && lhs.duration == rhs.duration &&
        lhs.chunksCatalog == rhs.chunksCatalog && lhs.fileIndex == rhs.fileIndex;
}

TestFileOperation generateFileOperation(int code)
{
    TestFileOperation result;

    result.startTime = nx::utils::random::number(1000LL, 439804651110LL);
    result.fileSize = nx::utils::random::number(0LL, 54975581388LL);
    result.code = code;
    result.cameraId = nx::utils::random::number(0, 65535);
    result.duration = nx::utils::random::number(0, 1048575);

    result.timeZone = nx::utils::random::number(0, 14) * 60;
    switch (nx::utils::random::number(0, 2))
    {
    case 0:
        break;
    case 1:
        result.timeZone += 30;
        break;
    case 2:
        result.timeZone += 45;
        break;
    }
    result.timeZone *= nx::utils::random::number(0, 1) == 0 ? 1 : -1;

    result.chunksCatalog = nx::utils::random::number(0, 1);
    result.fileIndex = nx::utils::random::number(0, 1) == 0
        ? DeviceFileCatalog::Chunk::FILE_INDEX_WITH_DURATION
        : DeviceFileCatalog::Chunk::FILE_INDEX_NONE;

    return result;
}

struct TestCameraOperation
{
    nx::media_db::RecordType code;
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
    TestCameraOperation result;

    result.code = nx::media_db::RecordType::CameraOperationAdd;
    result.uuidLen = nx::utils::random::number(20, 40);
    generateCameraUid(&result.camUniqueId, result.uuidLen);
    result.id = nx::utils::random::number(0, 65535);

    return result;
}

typedef boost::variant<TestFileHeader, TestFileOperation, TestCameraOperation> TestRecord;

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
            switch (nx::utils::random::number(0, 1))
            {
            case 0:
                switch (nx::utils::random::number(0, 1))
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
        else
        {
            qDebug() << "record not found";
        }
        return false;
    }

    TestDataVector dataVector;
};

class TestChunkManager
{
public:
    struct Catalog
    {
        QByteArray cameraUniqueId;
        QnServer::ChunksCatalog quality;
    };

    struct TestChunk
    {
        DeviceFileCatalog::Chunk chunk;
        bool isDeleted;
        bool isVisited;
        Catalog *catalog;
    };

    friend bool operator<(const TestChunk &lhs, const TestChunk &rhs)
    {
        return lhs.chunk.startTimeMs < rhs.chunk.startTimeMs;
    }

    typedef std::set<TestChunk> TestChunkCont;
    typedef std::vector<Catalog> CatalogCont;

public:
    TestChunkManager(int cameraCount)
    {
        for (int i = 0; i < cameraCount; ++i)
        {
            QByteArray camuuid;
            generateCameraUid(&camuuid, nx::utils::random::number(7, 15));
            Catalog c1 = { camuuid, QnServer::LowQualityCatalog };
            Catalog c2 = { camuuid, QnServer::HiQualityCatalog };

            m_catalogs.push_back(c1);
            m_catalogs.push_back(c2);
        }
    }

    boost::optional<TestChunk> generateAddOperation()
    {
        Catalog *catalog = &m_catalogs[nx::utils::random::number((size_t)0, m_catalogs.size() - 1)];
        TestFileOperation fileOp = generateFileOperation(0);

        if (chunkExists(fileOp.startTime))
            return boost::none;

        DeviceFileCatalog::Chunk chunk(fileOp.startTime, 1,  fileOp.fileIndex,
            fileOp.duration, fileOp.timeZone, (quint16)(fileOp.fileSize >> 32),
            (quint32)(fileOp.fileSize));

        TestChunk ret;
        ret.catalog = catalog;
        ret.isDeleted = false;
        ret.isVisited = false;
        ret.chunk = chunk;

        m_unremovedChunks.push_back((TestChunk*) &(*m_chunks.insert(ret).first));

        return ret;
    }

    TestChunk* generateRemoveOperation()
    {
        if (m_unremovedChunks.empty())
            return nullptr;

        auto indexToRemove = nx::utils::random::number((size_t)0, m_unremovedChunks.size() - 1);
        TestChunk* chunk = m_unremovedChunks[indexToRemove];
        chunk->isDeleted = true;
        m_unremovedChunks.erase(m_unremovedChunks.cbegin() + indexToRemove);

        return chunk;
    }

    std::pair<Catalog, std::deque<DeviceFileCatalog::Chunk>> generateReplaceOperation(int size)
    {
        Catalog *catalog = &m_catalogs[nx::utils::random::number((size_t)0, m_catalogs.size() - 1)];
        for (auto& chunk: m_chunks)
        {
            bool sameUniqueId = chunk.catalog->cameraUniqueId == catalog->cameraUniqueId;
            bool sameQuality = chunk.catalog->quality == catalog->quality;

            if (sameUniqueId && sameQuality)
                ((TestChunk&)chunk).isDeleted = true;
        }

        std::deque<DeviceFileCatalog::Chunk> ret;
        for (int i = 0; i < size; ++i)
        {
            TestFileOperation fileOp = generateFileOperation(0);
            if (chunkExists(fileOp.startTime))
                continue;

            DeviceFileCatalog::Chunk chunk(fileOp.startTime, 1, fileOp.fileIndex,
                fileOp.duration, fileOp.timeZone, (quint16)((fileOp.fileSize) >> 32),
                (quint32)(fileOp.fileSize));

            TestChunk tc;
            tc.catalog = catalog;
            tc.isDeleted = false;
            tc.isVisited = false;
            tc.chunk = chunk;

            m_chunks.insert(tc);
            ret.push_back(tc.chunk);
        }

        return std::make_pair(*catalog, ret);
    }

    TestChunkCont &get() { return m_chunks; }

private:
    TestChunkCont m_chunks;
    CatalogCont m_catalogs;
    std::vector<TestChunk*> m_unremovedChunks;

    bool chunkExists(qint64 startTime) const
    {
        TestChunk existingChunk;
        existingChunk.chunk.startTimeMs = startTime;

        if (m_chunks.find(existingChunk) != m_chunks.cend())
            return true;

        return false;
    }
};

class TestDbHelperHandler: boost::static_visitor<>
{
public:
    TestDbHelperHandler(TestDataManager *tdm): m_tdm(tdm) {}

    void operator()(const nx::media_db::CameraOperation &cameraOp)
    {
        TestCameraOperation top;
        top.camUniqueId = cameraOp.getCameraUniqueId();
        top.code = cameraOp.getRecordType();
        top.id = cameraOp.getCameraId();
        top.uuidLen = cameraOp.getCameraUniqueIdLen();

        ASSERT_TRUE(m_tdm->seekAndSet(top));
    }

    void operator()(const nx::media_db::MediaFileOperation &mediaFileOp)
    {
        TestFileOperation tfop;
        tfop.cameraId = mediaFileOp.getCameraId();
        tfop.chunksCatalog = mediaFileOp.getCatalog();
        tfop.code = (int)mediaFileOp.getRecordType();
        tfop.duration = mediaFileOp.getDuration();
        tfop.timeZone = mediaFileOp.getTimeZoneV2();
        tfop.fileSize = mediaFileOp.getFileSize();
        tfop.fileIndex = mediaFileOp.getFileTypeIndex();
        tfop.startTime = mediaFileOp.getStartTimeV2();

        ASSERT_TRUE(m_tdm->seekAndSet(tfop));
    }

private:
    TestDataManager *m_tdm;
    std::unique_ptr<std::string> m_errorString;
};


void initDbFile(QFile *dbFile, const QString &fileName)
{
    FILE* fd = fopen(fileName.toLatin1().constData(), "w+b");
    ASSERT_TRUE(fd != nullptr) << "DB file open failed: " << strerror(errno);
    dbFile->setFileName(fileName);
    ASSERT_TRUE(dbFile->open(fd, QIODevice::ReadWrite, QFileDevice::AutoCloseHandle))
        << strerror(errno);
}

void reopenDbFile(QFile *dbFile, const QString& fileName)
{
    ASSERT_TRUE(dbFile->isOpen()) << "Reopen failed. File is not opened";
    dbFile->close();
    FILE* fd = fopen(fileName.toLatin1().constData(), "r+b");
    ASSERT_TRUE(fd != nullptr) << "DB file " << fileName.toLatin1().constData()
                               << " open failed: " << strerror(errno);
    dbFile->setFileName(fileName);
    ASSERT_TRUE(dbFile->open(fd, QIODevice::ReadWrite, QFileDevice::AutoCloseHandle))
        << strerror(errno);
}

class MediaDbTest:
    public MediaServerModuleFixture
{
public:
    MediaDbTest()
    {
        platformAbstraction = std::make_unique<QnPlatformAbstraction>();
    }

    std::unique_ptr<QnPlatformAbstraction> platformAbstraction;
};

TEST(MediaFileOperations, BitsTwiddling)
{
    const size_t iterations = 10000;
    std::vector<TestFileHeader> fhVector;

    for (size_t i = 0; i < iterations; ++i)
        fhVector.push_back(generateFileHeader());

    for (size_t i = 0; i < iterations; ++i)
    {
        nx::media_db::FileHeader fh;
        fh.setDbVersion(fhVector[i].dbVersion);
        ASSERT_TRUE(fh.getDbVersion() == fhVector[i].dbVersion);
    }

    std::vector<TestFileOperation> fopVector;

    for (size_t i = 0; i < iterations;  ++i)
        fopVector.push_back(generateFileOperation(i % 2));

    for (size_t i = 0; i < iterations; ++i)
    {
        nx::media_db::MediaFileOperation mfop;
        const TestFileOperation &tfop = fopVector[i];

        mfop.setCameraId(tfop.cameraId);
        mfop.setCatalog(tfop.chunksCatalog);
        mfop.setDuration(tfop.duration);
        mfop.setTimeZone(tfop.timeZone);
        mfop.setFileSize(tfop.fileSize);
        mfop.setFileTypeIndex(tfop.fileIndex);
        mfop.setStartTime(tfop.startTime);
        mfop.setRecordType(nx::media_db::RecordType(tfop.code));

        ASSERT_TRUE(mfop.getCameraId() == tfop.cameraId);
        ASSERT_TRUE(mfop.getCatalog() == tfop.chunksCatalog);
        ASSERT_TRUE(mfop.getDuration() == tfop.duration);
        ASSERT_TRUE(mfop.getFileSize() == tfop.fileSize);
        ASSERT_TRUE(mfop.getFileTypeIndex() == tfop.fileIndex);
        ASSERT_TRUE(mfop.getRecordType() == nx::media_db::RecordType(tfop.code));
        ASSERT_TRUE(mfop.getStartTimeV2() == tfop.startTime);
        ASSERT_TRUE(mfop.getTimeZoneV2() == tfop.timeZone);
    }

    std::vector<TestCameraOperation> copVector;

    for (size_t i = 0; i < iterations;  ++i)
        copVector.push_back(generateCameraOperation());

    for (size_t i = 0; i < iterations; ++i)
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

TEST(MediaFileOperations, MediaFileOP_ResetValues)
{
    nx::media_db::MediaFileOperation mfop;
    mfop.setCameraId(65535);
    qint64 newCameraId = nx::utils::random::number<qint64>(0, 65534);
    mfop.setCameraId(newCameraId);
    ASSERT_EQ(newCameraId, mfop.getCameraId());

    mfop.setCatalog(1);
    mfop.setCatalog(0);
    ASSERT_EQ(0, mfop.getCatalog());

    mfop.setDuration(std::pow(2, 20));
    quint64 newDuration = nx::utils::random::number(0ULL, (quint64)(std::pow(2, 20) - 1));
    mfop.setDuration(newDuration);
    ASSERT_EQ(newDuration, mfop.getDuration());

    mfop.setFileSize(std::pow(2, 39));
    quint64 newFileSize = nx::utils::random::number(0ULL, (quint64)(std::pow(2, 39) - 1));
    mfop.setFileSize(newFileSize);
    ASSERT_EQ((qint64) newFileSize, mfop.getFileSize());

    mfop.setFileTypeIndex(DeviceFileCatalog::Chunk::FILE_INDEX_NONE);
    mfop.setFileTypeIndex(DeviceFileCatalog::Chunk::FILE_INDEX_WITH_DURATION);
    ASSERT_EQ(DeviceFileCatalog::Chunk::FILE_INDEX_WITH_DURATION, mfop.getFileTypeIndex());

    mfop.setRecordType(nx::media_db::RecordType::CameraOperationAdd);
    mfop.setRecordType(nx::media_db::RecordType::FileOperationAdd);
    ASSERT_EQ(nx::media_db::RecordType::FileOperationAdd, mfop.getRecordType());

    mfop.setStartTime(std::pow(2, 42));
    quint64 newStartTime = nx::utils::random::number(0ULL, (quint64)(std::pow(2, 42) - 1));
    mfop.setStartTime(newStartTime);
    ASSERT_EQ(newStartTime, mfop.getStartTimeV2());

    mfop.setStartTime(-1);
    ASSERT_EQ(-1, mfop.getStartTimeV2());
}

TEST(MediaFileOperations, CameraOP_ResetValues)
{
    nx::media_db::CameraOperation cop;
    cop.setCameraId(std::pow(2, 16));
    quint64 newCameraId = nx::utils::random::number(0ULL, (quint64)(std::pow(2, 16) - 1));
    cop.setCameraId(newCameraId);
    ASSERT_EQ(newCameraId, cop.getCameraId());

    cop.setCameraUniqueIdLen(std::pow(2, 14));
    quint64 newCameraUniqueIdLen = nx::utils::random::number(0ULL, (quint64)(std::pow(2, 14) - 1));
    cop.setCameraUniqueIdLen(newCameraUniqueIdLen);
    ASSERT_EQ(newCameraUniqueIdLen, cop.getCameraUniqueIdLen());
}

TEST_F(MediaDbTest, ReadWrite_Simple)
{
    nx::ut::utils::WorkDirResource workDirResource;
    ASSERT_TRUE((bool)workDirResource.getDirName());

    QString fileName = QDir(*workDirResource.getDirName()).absoluteFilePath("file.mdb");
    QFile dbFile;
    initDbFile(&dbFile, fileName);

    nx::media_db::Error error;
    TestDataManager tdm(10000);
    TestDbHelperHandler testHandler(&error, &tdm);
    nx::media_db::DbHelper dbHelper(&testHandler);
    dbHelper.setDevice(&dbFile);

    // Wrong mode test
    error = dbHelper.writeFileHeader(
        boost::get<TestFileHeader>(
            tdm.dataVector[0].data).dbVersion);

    ASSERT_TRUE(error == nx::media_db::Error::WrongMode);

    dbHelper.setMode(nx::media_db::Mode::Write);

    for (auto &data : tdm.dataVector)
        boost::apply_visitor(RecordWriteVisitor(&dbHelper), data.data);

    dbHelper.setMode(nx::media_db::Mode::Read);
    ASSERT_TRUE(testHandler.getErrorString() == nullptr) << *testHandler.getErrorString();
    testHandler.resetErrorString();

    dbHelper.reset();
    reopenDbFile(&dbFile, fileName);

    uint8_t dbVersion;

    error = dbHelper.readFileHeader(&dbVersion);

    ASSERT_TRUE(dbVersion == boost::get<TestFileHeader>(tdm.dataVector[0].data).dbVersion);
    tdm.dataVector[0].visited = true;
    ASSERT_TRUE(error == nx::media_db::Error::NoError);

    while (error == nx::media_db::Error::NoError)
    {
        const auto dbRecord = dbHelper.readRecord(&error);
        boost::apply_visitor(testHandler, dbRecord);
    }

    dbFile.close();
    ASSERT_TRUE(QFile::remove(fileName));

    ASSERT_TRUE(
        std::all_of(
            tdm.dataVector.cbegin(),
            tdm.dataVector.cend(),
            [](const TestData &td) { return td.visited; }));
}

TEST_F(MediaDbTest, DbFileTruncate)
{
    nx::ut::utils::WorkDirResource workDirResource;
    ASSERT_TRUE((bool)workDirResource.getDirName());

    QString fileName = QDir(*workDirResource.getDirName()).absoluteFilePath("file.mdb");
    QFile::remove(fileName);
    int truncateCount = 1000;

    while (truncateCount-- >= 0)
    {
        QFile dbFile;
        initDbFile(&dbFile, fileName);

        nx::media_db::Error error;
        TestDataManager tdm(1);
        TestDbHelperHandler testHandler(&error, &tdm);
        nx::media_db::DbHelper dbHelper(&testHandler);

        dbHelper.setDevice(&dbFile);
        dbHelper.setMode(nx::media_db::Mode::Write);

        for (auto &data : tdm.dataVector)
            boost::apply_visitor(RecordWriteVisitor(&dbHelper), data.data);

        dbHelper.setMode(nx::media_db::Mode::Read);
        ASSERT_TRUE(testHandler.getErrorString() == nullptr) << *testHandler.getErrorString();
        testHandler.resetErrorString();

        reopenDbFile(&dbFile, fileName);

        QByteArray content = dbFile.readAll();
        ASSERT_NE(content.size(), 0);
        dbFile.close();
        ASSERT_TRUE(QFile::remove(fileName)) << "DB file "
                                     << fileName.toLatin1().constData()
                                     << " remove failed";
        // truncating randomly last record
        content.truncate((int)content.size() - nx::utils::random::number((size_t)1, sizeof(qint64) * 2 - 2));

        initDbFile(&dbFile, fileName);
        dbFile.write(content);

        reopenDbFile(&dbFile, fileName);
        dbHelper.setDevice(&dbFile);

        uint8_t dbVersion;
        error = dbHelper.readFileHeader(&dbVersion);

        ASSERT_TRUE(dbVersion == boost::get<TestFileHeader>(tdm.dataVector[0].data).dbVersion);
        tdm.dataVector[0].visited = true;
        ASSERT_TRUE(error == nx::media_db::Error::NoError) << (int)error;

        while (error == nx::media_db::Error::NoError)
        {
            const auto dbRecord = dbHelper.readRecord(&error);
            if (error == nx::media_db::Error::Eof)
                qDebug() << "EOF";
            else if (error == nx::media_db::Error::ReadError)
                qDebug() << "Read error";

            if (error == nx::media_db::Error::NoError || error == nx::media_db::Error::Eof)
                boost::apply_visitor(testHandler, dbRecord);
        }

        dbFile.close();
        //ASSERT_TRUE(error == nx::media_db::Error::ReadError) << (int)error << truncateCount;
        //ASSERT_TRUE(QFile::remove(fileName));

        size_t readRecords = std::count_if(tdm.dataVector.cbegin(), tdm.dataVector.cend(),
                                           [](const TestData &td) { return td.visited; });
        // we've read all except the very last record
        ASSERT_TRUE(readRecords == tdm.dataVector.size() - 1) << readRecords << truncateCount;
    }
}

TEST_F(MediaDbTest, ReadWrite_MT)
{
    nx::ut::utils::WorkDirResource workDirResource;
    ASSERT_TRUE((bool)workDirResource.getDirName());

    QString fileName = QDir(*workDirResource.getDirName()).absoluteFilePath("file.mdb");
    QFile dbFile;
    initDbFile(&dbFile, fileName);

    const size_t threadsNum = 4;
    const size_t recordsCount = 1000;

    nx::media_db::Error error;
    TestDataManager tdm(threadsNum * recordsCount);
    TestDbHelperHandler testHandler(&error, &tdm);
    nx::media_db::DbHelper dbHelper(&testHandler);

    dbHelper.setDevice(&dbFile);
    dbHelper.setMode(nx::media_db::Mode::Write);

    // write header explicitly
    boost::apply_visitor(RecordWriteVisitor(&dbHelper), tdm.dataVector[0].data);

    std::vector<nx::utils::thread> threads;
    for (size_t i = 0; i < threadsNum; ++i)
    {
        threads.emplace_back(
            nx::utils::thread([&dbHelper, &tdm, i, recordsCount]
                        {
                            size_t j = i * recordsCount + 1;
                            for (; j < (i + 1) * recordsCount + 1; ++j)
                                boost::apply_visitor(RecordWriteVisitor(&dbHelper),
                                                     tdm.dataVector[j].data);
                        }));
    }

    for (size_t i = 0; i < threadsNum; ++i)
        if (threads[i].joinable())
            threads[i].join();


    dbHelper.setMode(nx::media_db::Mode::Read);
    dbHelper.reset();
    ASSERT_TRUE(testHandler.getErrorString() == nullptr) << *testHandler.getErrorString();
    testHandler.resetErrorString();
    reopenDbFile(&dbFile, fileName);

    uint8_t dbVersion;
    error = dbHelper.readFileHeader(&dbVersion);

    ASSERT_TRUE(dbVersion == boost::get<TestFileHeader>(tdm.dataVector[0].data).dbVersion);
    tdm.dataVector[0].visited = true;
    ASSERT_TRUE(error == nx::media_db::Error::NoError);

    while (error == nx::media_db::Error::NoError)
    {
        const auto dbRecord = dbHelper.readRecord(&error);
        boost::apply_visitor(testHandler, dbRecord);
    }

    ASSERT_TRUE(testHandler.getErrorString() == nullptr) << *testHandler.getErrorString();
    testHandler.resetErrorString();

    size_t readRecords = std::count_if(tdm.dataVector.cbegin(), tdm.dataVector.cend(),
                                       [](const TestData &td) { return td.visited; });
    ASSERT_TRUE(readRecords == tdm.dataVector.size());

    // Write again after we've read till the end
    dbHelper.setMode(nx::media_db::Mode::Write);

    threads.clear();
    for (size_t i = 0; i < threadsNum; ++i)
    {
            threads.emplace_back(
                nx::utils::thread([&dbHelper, &tdm, i, recordsCount]
                            {
                                size_t j = i * recordsCount + 1;
                                for (; j < (i + 1) * recordsCount + 1; ++j)
                                    boost::apply_visitor(RecordWriteVisitor(&dbHelper),
                                                         tdm.dataVector[j].data);
                            }));
    }

    for (size_t i = 0; i < threadsNum; ++i)
        if (threads[i].joinable())
            threads[i].join();

    // and now read again from the beginning
    dbHelper.setMode(nx::media_db::Mode::Read); // wait for writer here
    reopenDbFile(&dbFile, fileName);

    error = dbHelper.readFileHeader(&dbVersion);
    ASSERT_TRUE(dbVersion == boost::get<TestFileHeader>(tdm.dataVector[0].data).dbVersion);
    tdm.dataVector[0].visited = true;
    ASSERT_TRUE(error == nx::media_db::Error::NoError);

    size_t copySize = tdm.dataVector.size();
    for (size_t i = 1; i < copySize; ++i)
    {   // double test data. we should find every record
        tdm.dataVector[i].visited = false;
        tdm.dataVector.push_back(tdm.dataVector[i]);
    }

    size_t readCount = 0;
    while (error == nx::media_db::Error::NoError)
    {
        ++readCount;
        const auto dbRecord = dbHelper.readRecord(&error);
        boost::apply_visitor(testHandler, dbRecord);
    }

    readRecords = std::count_if(tdm.dataVector.cbegin(), tdm.dataVector.cend(),
                                [](const TestData &td) { return td.visited; });
    ASSERT_TRUE(readRecords == tdm.dataVector.size()) << readRecords;
    dbFile.close();
    QFile::remove(fileName);
}

TEST_F(MediaDbTest, StorageDB)
{
    nx::ut::utils::WorkDirResource workDirResource;
    ASSERT_TRUE((bool)workDirResource.getDirName());

    const QString workDirPath = *workDirResource.getDirName();

    bool result;
    QnFileStorageResourcePtr storage(new QnFileStorageResource(&serverModule()));
    storage->setUrl(workDirPath);
    result = storage->initOrUpdate() == Qn::StorageInit_Ok;
    ASSERT_TRUE(result);

    QnStorageDb sdb(&serverModule(), storage, 1);
    result = sdb.open(workDirPath + lit("/test.nxdb"));
    ASSERT_TRUE(result);

    sdb.loadFullFileCatalog();

    QnMutex mutex;
    std::vector<nx::utils::thread> threads;
    TestChunkManager tcm(128);

    //auto writerFunc = [&mutex, &sdb, &tcm]
    //{
    //    for (int i = 0; i < 6000000; ++i)
    //    {
    //        int diceRoll = nx::utils::random::number(0, 10);
    //        errorStream << "dice rolled: " << diceRoll << std::endl;
    //        switch (diceRoll)
    //        {
    //            //case 0:
    //            //{
    //            //    std::pair<TestChunkManager::Catalog, std::deque<DeviceFileCatalog::Chunk>> p;
    //            //    QnMutexLocker lk(&mutex);
    //            //    p = tcm.generateReplaceOperation(nx::utils::random::number(10, 100));
    //            //    sdb.replaceChunks(p.first.cameraUniqueId, p.first.quality, p.second);
    //            //    break;
    //            //}
    //            //case 1:
    //            case 0:
    //            case 1:
    //            case 2:
    //            {
    //                TestChunkManager::TestChunk *chunk;
    //                QnMutexLocker lk(&mutex);
    //                chunk = tcm.generateRemoveOperation();
    //                if (chunk)
    //                {
    //                    sdb.deleteRecords(
    //                        chunk->catalog->cameraUniqueId,
    //                        chunk->catalog->quality, chunk->chunk.startTimeMs);
    //                }
    //                break;
    //            }
    //            default:
    //            {
    //                boost::optional<TestChunkManager::TestChunk> chunk;
    //                QnMutexLocker lk(&mutex);
    //                chunk = tcm.generateAddOperation();
    //                if (!(bool)chunk)
    //                    break;
    //                sdb.addRecord(
    //                    chunk->catalog->cameraUniqueId,
    //                    chunk->catalog->quality, chunk->chunk);
    //                break;
    //            }
    //        }

    //        if (i % 10000 == 0)
    //            qDebug() << "Added" << i << "records";
    //    }
    //};

    for (int i = 0; i < 6000; ++i)
    {
        //int diceRoll = nx::utils::random::number(0, 10);
        //errorStream << "dice rolled: " << diceRoll << std::endl;
        switch (i % 10)
        {
            //case 0:
            //{
            //    std::pair<TestChunkManager::Catalog, std::deque<DeviceFileCatalog::Chunk>> p;
            //    QnMutexLocker lk(&mutex);
            //    p = tcm.generateReplaceOperation(nx::utils::random::number(10, 100));
            //    sdb.replaceChunks(p.first.cameraUniqueId, p.first.quality, p.second);
            //    break;
            //}
            //case 1:
        case 0:
        case 1:
        case 2:
        {
            TestChunkManager::TestChunk *chunk;
            chunk = tcm.generateRemoveOperation();
            if (chunk)
            {
                sdb.deleteRecords(
                    chunk->catalog->cameraUniqueId,
                    chunk->catalog->quality, chunk->chunk.startTimeMs);
            }
            break;
        }
        default:
        {
            boost::optional<TestChunkManager::TestChunk> chunk;
            chunk = tcm.generateAddOperation();
            if (!(bool)chunk)
                break;
            sdb.addRecord(
                chunk->catalog->cameraUniqueId,
                chunk->catalog->quality, chunk->chunk);
            break;
        }
        }

        if (i % 10000 == 0)
            qDebug() << "Added" << i << "records";
    }

    using namespace std::chrono;

    QVector<DeviceFileCatalogPtr> dbChunkCatalogs;
    auto readerFunc = [&dbChunkCatalogs, &mutex, &tcm, &sdb]
    {
        //std::this_thread::sleep_for(std::chrono::milliseconds(500));
        for (size_t i = 0; i < 1; ++i)
        {
            const auto start = system_clock::now();
            sdb.loadFullFileCatalog();
            const auto end = system_clock::now();
            qDebug() << "VACUUM FINISHED" << duration_cast<milliseconds>(end - start).count() << "ms";
            //std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    };

    //for (size_t i = 0; i < 3; ++i)
    //    threads.push_back(nx::utils::thread(writerFunc));

    ////threads.push_back(nx::utils::thread(readerFunc));

    //for (auto &t : threads)
    //    t.join();

    qDebug() << "WRITERS FINISHED";

    nx::utils::thread(readerFunc).join();
    return;

    dbChunkCatalogs = sdb.loadFullFileCatalog();

    for (auto catalogIt = dbChunkCatalogs.cbegin();
         catalogIt != dbChunkCatalogs.cend();
         ++catalogIt)
    {
        for (auto chunkIt = (*catalogIt)->getChunksUnsafe().cbegin();
             chunkIt != (*catalogIt)->getChunksUnsafe().cend();
             ++chunkIt)
        {
            TestChunkManager::TestChunkCont::iterator tcmIt =
                std::find_if(tcm.get().begin(), tcm.get().end(),
                    [catalogIt, chunkIt](const TestChunkManager::TestChunk &tc)
                    {
                        return tc.catalog->cameraUniqueId == (*catalogIt)->cameraUniqueId()
                            && tc.catalog->quality == (*catalogIt)->getCatalog()
                            && !tc.isVisited && !tc.isDeleted && tc.chunk == *chunkIt;
                    });
            bool tcmChunkFound = tcmIt != tcm.get().end();
            ASSERT_TRUE(tcmChunkFound);
            if (tcmChunkFound)
                ((TestChunkManager::TestChunk&)(*tcmIt)).isVisited = true;
        }
    }

    bool allVisited = std::none_of(tcm.get().begin(), tcm.get().end(),
                                   [](const TestChunkManager::TestChunk &tc)
                                   {
                                       return !tc.isDeleted && !tc.isVisited;
                                   });
    if (!allVisited)
    {
//        std::cout << errorStream.stream->str() << std::endl;
//        errorStream.reset();
        size_t notVisited = std::count_if(
                tcm.get().cbegin(),
                tcm.get().cend(),
                [](const TestChunkManager::TestChunk &tc)
                { return !tc.isDeleted && !tc.isVisited; });
        qWarning() << lit("Not visited count: %1").arg(notVisited);
    }

    ASSERT_EQ(allVisited, true);
}

TEST_F(MediaDbTest, Migration_from_sqlite)
{
    nx::ut::utils::WorkDirResource workDirResource;
    ASSERT_TRUE((bool)workDirResource.getDirName());

    const QString workDirPath = *workDirResource.getDirName();
    QString simplifiedGUID = serverModule().commonModule()->moduleGUID().toSimpleString();
    QString fileName = closeDirPath(workDirPath) + QString::fromLatin1("%1_media.sqlite").arg(simplifiedGUID);
    //QString fileName = closeDirPath(workDirPath) + lit("media.sqlite");
    auto sqlDb = std::unique_ptr<QSqlDatabase>(
            new QSqlDatabase(
                QSqlDatabase::addDatabase(
                    lit("QSQLITE"),
                    QString("QnStorageManager_%1").arg(fileName))));

    sqlDb->setDatabaseName(fileName);
    ASSERT_TRUE(sqlDb->open());
    ASSERT_TRUE(nx::sql::SqlQueryExecutionHelper::execSQLFile(lit(":/01_create_storage_db.sql"), *sqlDb));

    const size_t kMaxCatalogs = 4;
    const size_t kMaxChunks = 50;
    const size_t k23MaxChunks = kMaxChunks * 2 / 3;
    const size_t k13MaxChunks = kMaxChunks / 3;
    std::vector<DeviceFileCatalogPtr> referenceCatalogs;

    for (size_t i = 0; i < kMaxCatalogs; ++i)
    {
        auto catalog = DeviceFileCatalogPtr(new DeviceFileCatalog(
            &serverModule(),
            QString::number(i),
            i % 2 ? QnServer::LowQualityCatalog : QnServer::HiQualityCatalog,
            QnServer::StoragePool::Normal));
        std::deque<DeviceFileCatalog::Chunk> chunks;
        for (size_t j = 0; j < kMaxChunks; ++j)
            chunks.push_back(DeviceFileCatalog::Chunk((j + 10) * j + 10, 0, DeviceFileCatalog::Chunk::FILE_INDEX_WITH_DURATION, 1, 0, 1, 1));
        catalog->addChunks(chunks);

        referenceCatalogs.push_back(catalog);
    }

    /*  Two thirds of reference chunks (from the beginning) we will write to sqlite db and two thirds (from the end) - to the new media db.
    *   After migration routine executed, result should be equal to the reference catalogs.
    */

    for (size_t i = 0; i < kMaxCatalogs; ++i)
    {
        for (size_t j = 0; j < k23MaxChunks; ++j)
        {
            QSqlQuery query(*sqlDb);
            ASSERT_TRUE(query.prepare("INSERT OR REPLACE INTO storage_data values(?,?,?,?,?,?,?)"));
            DeviceFileCatalog::Chunk const &chunk = referenceCatalogs[i]->getChunksUnsafe().at(j);

            query.addBindValue(referenceCatalogs[i]->cameraUniqueId()); // unique_id
            query.addBindValue(referenceCatalogs[i]->getCatalog()); // role
            query.addBindValue(chunk.startTimeMs); // start_time
            query.addBindValue(chunk.timeZone); // timezone
            query.addBindValue(chunk.fileIndex); // file_index
            query.addBindValue(chunk.durationMs); // duration
            query.addBindValue(chunk.getFileSize()); // filesize

            ASSERT_TRUE(query.exec());
        }
    }

    bool result;
    QnFileStorageResourcePtr storage(new QnFileStorageResource(&serverModule()));
    storage->setUrl(workDirPath);
    result = storage->initOrUpdate() == Qn::StorageInit_Ok;
    ASSERT_TRUE(result);

    auto connectionName = sqlDb->connectionName();
    sqlDb->close();
    sqlDb.reset();
    QSqlDatabase::removeDatabase(connectionName);

    auto sdb = serverModule().storageDbPool()->getSDB(storage);
    ASSERT_TRUE(result);
    sdb->loadFullFileCatalog();

    for (size_t i = 0; i < kMaxCatalogs; ++i)
    {
        for (size_t j = k13MaxChunks; j < kMaxChunks; ++j)
        {
            sdb->addRecord(referenceCatalogs[i]->cameraUniqueId(),
                           referenceCatalogs[i]->getCatalog(),
                           referenceCatalogs[i]->getChunksUnsafe().at(j));
        }
    }

    serverModule().normalStorageManager()->migrateSqliteDatabase(storage);
    auto mergedCatalogs = sdb->loadFullFileCatalog();

    for (size_t i = 0; i < kMaxCatalogs; ++i)
    {
        auto mergedIt = std::find_if(mergedCatalogs.cbegin(), mergedCatalogs.cend(),
                                     [&referenceCatalogs, i](const DeviceFileCatalogPtr &c)
                                     { return c->cameraUniqueId() == referenceCatalogs[i]->cameraUniqueId() &&
                                              c->getCatalog() == referenceCatalogs[i]->getCatalog(); });
        ASSERT_TRUE(mergedIt != mergedCatalogs.cend());
        auto left = (*mergedIt)->getChunksUnsafe();
        auto right = referenceCatalogs[i]->getChunksUnsafe();
        ASSERT_TRUE(left == right);

    }
}

