#define GTEST_HAS_TR1_TUPLE     0
#define GTEST_USE_OWN_TR1_TUPLE 1

#define GTEST_HAS_POSIX_RE 0
#include <gtest/gtest.h>

#include <random>
#include <climits>
#include <vector>
#include <algorithm>

#include "plugins/storage/file_storage/file_storage_resource.h"
#include <recorder/storage_manager.h>
#include <plugins/storage/file_storage/file_storage_resource.h>
#include <core/resource_management/status_dictionary.h>
#include <common/common_module.h>
#include <core/resource_management/resource_properties.h>
#include <utils/common/util.h>
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
    int timeZone;
    int chunksCatalog;
};

bool operator == (const TestFileOperation &lhs, const TestFileOperation &rhs)
{
    return lhs.startTime == rhs.startTime && lhs.fileSize == rhs.fileSize &&
           lhs.code == rhs.code && lhs.cameraId == rhs.cameraId &&
           lhs.timeZone == rhs.timeZone && lhs.duration == rhs.duration && 
           lhs.chunksCatalog == rhs.chunksCatalog;
}

TestFileOperation generateFileOperation(int code)
{
    int tz = genRandomNumber<0, 14>() * 60;
    switch (genRandomNumber<0, 2>())
    {
    case 0:
        break;
    case 1:
        tz += 30;
        break;
    case 2:
        tz += 45;
        break;
    }
    tz *= genRandomNumber<0, 1>() == 0 ? 1 : -1;

    return{ genRandomNumber<1000, 439804651110LL>(), genRandomNumber<0, 1099511627776LL>(),
            code, (int)genRandomNumber<0, 65535>(), (int)genRandomNumber<0, 1048575LL>(), 
            tz, (int)genRandomNumber<0, 1>() };
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
        fileOp.setTimeZone(tfo.timeZone);
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

    typedef std::vector<TestChunk> TestChunkCont;
    typedef std::vector<Catalog> CatalogCont;

public:
    TestChunkManager(int cameraCount)
    {
        for (int i = 0; i < cameraCount; ++i)
        {
            QByteArray camuuid;
            generateCameraUid(&camuuid, genRandomNumber<7, 15>());
            Catalog c1 = { camuuid, QnServer::LowQualityCatalog };
            Catalog c2 = { camuuid, QnServer::HiQualityCatalog };
            
            m_catalogs.push_back(c1);
            m_catalogs.push_back(c2);
        }
    }

    TestChunk generateAddOperation()
    {   
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<qint64> dist(0, m_catalogs.size() - 1);

        Catalog *catalog = &m_catalogs[dist(gen)];
        TestFileOperation fileOp = generateFileOperation(0);
        DeviceFileCatalog::Chunk chunk(fileOp.startTime, 1,
                                       DeviceFileCatalog::Chunk::FILE_INDEX_WITH_DURATION,
                                       fileOp.duration, fileOp.timeZone,
                                       (quint16)(fileOp.fileSize >> 32),
                                       (quint32)(fileOp.fileSize));

        TestChunk ret;
        ret.catalog = catalog;
        ret.isDeleted = false;
        ret.isVisited = false;
        ret.chunk = chunk;

        m_chunks.push_back(ret);

        return ret;
    }

    TestChunk *generateRemoveOperation()
    {
        std::vector<TestChunk *> unremovedChunks;
        for (size_t i = 0; i < m_chunks.size(); ++i)
        {
            if (m_chunks[i].isDeleted == false)
                unremovedChunks.push_back(&m_chunks[i]);
        }
        if (unremovedChunks.empty())
            return nullptr;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<qint64> dist(0, unremovedChunks.size() - 1);

        TestChunk *chunk = unremovedChunks[dist(gen)];
        chunk->isDeleted = true;
        return chunk;
    }

    std::pair<Catalog, std::deque<DeviceFileCatalog::Chunk>> generateReplaceOperation(int size)
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<qint64> dist(0, m_catalogs.size() - 1);

        Catalog *catalog = &m_catalogs[dist(gen)];
        for (size_t i = 0; i < m_chunks.size(); ++i)
        {
            if (m_chunks[i].catalog->cameraUniqueId == catalog->cameraUniqueId &&
                m_chunks[i].catalog->quality == catalog->quality)
            {
                m_chunks[i].isDeleted = true;
            }
        }

        std::deque<DeviceFileCatalog::Chunk> ret;
        for (int i = 0; i < size; ++i)
        {
            TestFileOperation fileOp = generateFileOperation(0);
            DeviceFileCatalog::Chunk chunk(fileOp.startTime, 1,
                                           DeviceFileCatalog::Chunk::FILE_INDEX_WITH_DURATION,
                                           fileOp.duration, fileOp.timeZone,
                                           (quint16)((fileOp.fileSize) >> 32),
                                           (quint32)(fileOp.fileSize));

            TestChunk tc;
            tc.catalog = catalog;
            tc.isDeleted = false;
            tc.isVisited = false;
            tc.chunk = chunk;

            m_chunks.push_back(tc);
            ret.push_back(tc.chunk);
        }

        return std::make_pair(*catalog, ret);
    }

    TestChunkCont &get() { return m_chunks; }

private:
    TestChunkCont m_chunks;
    CatalogCont m_catalogs;
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
        tfop.timeZone = mediaFileOp.getTimeZone();
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
        mfop.setTimeZone(tfop.timeZone);
        mfop.setFileSize(tfop.fileSize);
        mfop.setStartTime(tfop.startTime);
        mfop.setRecordType(nx::media_db::RecordType(tfop.code));

        ASSERT_TRUE(mfop.getCameraId() == tfop.cameraId);
        ASSERT_TRUE(mfop.getCatalog() == tfop.chunksCatalog);
        ASSERT_TRUE(mfop.getDuration() == tfop.duration);
        ASSERT_TRUE(mfop.getFileSize() == tfop.fileSize);
        ASSERT_TRUE(mfop.getRecordType() == nx::media_db::RecordType(tfop.code));
        ASSERT_TRUE(mfop.getStartTime() == tfop.startTime);
        ASSERT_TRUE(mfop.getTimeZone() == tfop.timeZone);
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

TEST(MediaDb_test, ReadWrite_Simple)
{
    const QString workDirPath = qApp->applicationDirPath() + lit("/tmp/media_db");
    QFile dbFile;
    initDbFile(&dbFile);

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
        nx::media_db::DbHelper dbHelper(&testHandler);

        dbHelper.setDevice(&dbFile);
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
        // we've read all except the very last record
        ASSERT_TRUE(readRecords == tdm.dataVector.size() - 1) << readRecords;
    }
    recursiveClean(workDirPath);
}

TEST(MediaDb_test, ReadWrite_MT)
{
    const QString workDirPath = qApp->applicationDirPath() + lit("/tmp/media_db");
    QFile dbFile;
    initDbFile(&dbFile);

    const size_t threadsNum = 4;
    const size_t recordsCount = 1000;

    nx::media_db::Error error;
    TestDataManager tdm(threadsNum * recordsCount);
    TestDbHelperHandler testHandler(&error, &tdm);
    nx::media_db::DbHelper dbHelper(&testHandler);

    dbHelper.setDevice(&dbFile);
    dbHelper.setMode(nx::media_db::Mode::Write);

    //write header explicitely
    boost::apply_visitor(RecordWriteVisitor(&dbHelper), tdm.dataVector[0].data);

    std::vector<std::thread> threads;
    for (size_t i = 0; i < threadsNum; ++i)
    {
            threads.emplace_back(
                std::thread([&dbHelper, &tdm, i, recordsCount]
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
    dbFile.close();
    dbFile.open(QIODevice::ReadWrite);

    uint8_t dbVersion;
    error = dbHelper.readFileHeader(&dbVersion);

    ASSERT_TRUE(dbVersion == boost::get<TestFileHeader>(tdm.dataVector[0].data).dbVersion);
    tdm.dataVector[0].visited = true;
    ASSERT_TRUE(error == nx::media_db::Error::NoError);

    while (error == nx::media_db::Error::NoError)
        dbHelper.readRecord();

    size_t readRecords = std::count_if(tdm.dataVector.cbegin(), tdm.dataVector.cend(),
                                       [](const TestData &td) { return td.visited; });
    ASSERT_TRUE(readRecords == tdm.dataVector.size());

    // Write again after we've read till the end
    dbHelper.setMode(nx::media_db::Mode::Write);

    threads.clear();
    for (size_t i = 0; i < threadsNum; ++i)
    {
            threads.emplace_back(
                std::thread([&dbHelper, &tdm, i, recordsCount]
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
    dbFile.close();
    dbFile.open(QIODevice::ReadWrite);

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
        dbHelper.readRecord();
    }

    readRecords = std::count_if(tdm.dataVector.cbegin(), tdm.dataVector.cend(),
                                [](const TestData &td) { return td.visited; });
    ASSERT_TRUE(readRecords == tdm.dataVector.size()) << readRecords;
    dbFile.close();
    recursiveClean(workDirPath);
}

TEST(MediaDb_test, StorageDB)
{
    const QString workDirPath = qApp->applicationDirPath() + lit("/tmp/media_db");

    std::unique_ptr<QnCommonModule> commonModule;
    if (!qnCommon) {
        commonModule = std::unique_ptr<QnCommonModule>(new QnCommonModule);
    }
    commonModule->setModuleGUID("{A680980C-70D1-4545-A5E5-72D89E33648B}");

    std::unique_ptr<QnResourceStatusDictionary> statusDictionary;
    if (!qnStatusDictionary) {
        statusDictionary = std::unique_ptr<QnResourceStatusDictionary>(
                new QnResourceStatusDictionary);
    }

    std::unique_ptr<QnResourcePropertyDictionary> propDictionary;
    if (!propertyDictionary) {
        propDictionary = std::unique_ptr<QnResourcePropertyDictionary>(
            new QnResourcePropertyDictionary);
    }

    std::unique_ptr<QnStorageDbPool> dbPool;
    if (!qnStorageDbPool) {
        dbPool = std::unique_ptr<QnStorageDbPool>(new QnStorageDbPool);
    }

    QnFileStorageResourcePtr storage(new QnFileStorageResource);
    storage->setUrl(workDirPath);

    QnStorageDb sdb(storage, 1);
    bool result = sdb.open(workDirPath + lit("/test.nxdb"));
    ASSERT_TRUE(result);
    
    sdb.loadFullFileCatalog();

    QnMutex mutex;
    std::vector<std::thread> threads;
    TestChunkManager tcm(128);
    
    auto writerFunc = [&mutex, &sdb, &tcm]
    {
        for (int i = 0; i < 500; ++i)
        {
            int diceRoll = genRandomNumber<0, 10>();
            switch (diceRoll)
            {
            case 0:
                {
                    std::pair<TestChunkManager::Catalog, std::deque<DeviceFileCatalog::Chunk>> p;
                    {
                        QnMutexLocker lk(&mutex);
                        p = tcm.generateReplaceOperation(genRandomNumber<10, 100>());
                    }
                    sdb.replaceChunks(p.first.cameraUniqueId, p.first.quality, p.second);
                }
            break;
            case 1:
            case 2:
                {
                    TestChunkManager::TestChunk *chunk;
                    {
                        QnMutexLocker lk(&mutex);
                        chunk = tcm.generateRemoveOperation();
                    }
                    if (chunk)
                        sdb.deleteRecords(chunk->catalog->cameraUniqueId,
                                          chunk->catalog->quality,
                                          chunk->chunk.startTimeMs);
                }
                break;
            default:
                {
                    TestChunkManager::TestChunk chunk;
                    {
                        QnMutexLocker lk(&mutex);
                        chunk = tcm.generateAddOperation();
                    }
                    sdb.addRecord(chunk.catalog->cameraUniqueId,
                                  chunk.catalog->quality,
                                  chunk.chunk);
                }
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    };

    QVector<DeviceFileCatalogPtr> dbChunkCatalogs;
    auto readerFunc = [&dbChunkCatalogs, &mutex, &tcm, &sdb]
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        for (size_t i = 0; i < 5; ++i)
        {
            dbChunkCatalogs = sdb.loadFullFileCatalog();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    };

    for (size_t i = 0; i < 3; ++i)
        threads.push_back(std::thread(writerFunc));

    threads.push_back(std::thread(readerFunc));

    for (auto &t : threads)
        t.join();

    dbChunkCatalogs = sdb.loadFullFileCatalog();

    for (auto catalogIt = dbChunkCatalogs.cbegin(); 
         catalogIt != dbChunkCatalogs.cend();
         ++catalogIt)
    {
        for (auto chunkIt = (*catalogIt)->getChunks().cbegin();
             chunkIt != (*catalogIt)->getChunks().cend();
             ++chunkIt)
        {
            TestChunkManager::TestChunkCont::iterator tcmIt = 
                std::find_if(tcm.get().begin(), 
                             tcm.get().end(),
                             [catalogIt, chunkIt](const TestChunkManager::TestChunk &tc)
                             {
                                 return tc.catalog->cameraUniqueId == (*catalogIt)->cameraUniqueId() &&
                                        tc.catalog->quality == (*catalogIt)->getCatalog() &&
                                        !tc.isVisited && !tc.isDeleted && tc.chunk == *chunkIt;
                             });
            bool tcmChunkFound = tcmIt != tcm.get().end();
            EXPECT_TRUE(tcmChunkFound);
            if (tcmChunkFound)
                tcmIt->isVisited = true;
        }
    }

    bool allVisited = std::none_of(tcm.get().begin(), tcm.get().end(), 
                                   [](const TestChunkManager::TestChunk &tc) 
                                   {
                                       return !tc.isDeleted && !tc.isVisited; 
                                   });
    ASSERT_EQ(allVisited, true);

    recursiveClean(workDirPath);
}

TEST(MediaDb_test, Cleanup)
{
    const QString workDirPath = qApp->applicationDirPath() + lit("/tmp/media_db");
    recursiveClean(workDirPath);
}