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
#include <boost/variant/multivisitors.hpp>

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
#include <nx/utils/std/thread.h>

namespace nx::media_db::test {
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
    RecordType type;
    int cameraId;
    int duration;
    int timeZone;
    int chunksCatalog;
    int fileIndex;
};

bool operator == (const TestFileOperation &lhs, const TestFileOperation &rhs)
{
    return lhs.startTime == rhs.startTime && lhs.fileSize == rhs.fileSize &&
        lhs.type == rhs.type && lhs.cameraId == rhs.cameraId &&
        lhs.timeZone == rhs.timeZone && lhs.duration == rhs.duration &&
        lhs.chunksCatalog == rhs.chunksCatalog && lhs.fileIndex == rhs.fileIndex;
}

TestFileOperation generateFileOperation(RecordType type)
{
    TestFileOperation result;

    result.startTime = nx::utils::random::number(1000LL, 439804651110LL);
    result.fileSize = nx::utils::random::number(0LL, 54975581388LL);
    result.type = type;
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

nx::media_db::MediaFileOperation fromTestFileOperation(const TestFileOperation& testFileOperation)
{
    nx::media_db::MediaFileOperation mfop;

    mfop.setCameraId(testFileOperation.cameraId);
    mfop.setCatalog(testFileOperation.chunksCatalog);
    mfop.setDuration(testFileOperation.duration);
    mfop.setTimeZone(testFileOperation.timeZone);
    mfop.setFileSize(testFileOperation.fileSize);
    mfop.setFileTypeIndex(testFileOperation.fileIndex);
    mfop.setStartTime(testFileOperation.startTime);
    mfop.setRecordType(nx::media_db::RecordType(testFileOperation.type));

    return mfop;
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

nx::media_db::CameraOperation fromTestCameraOperation(
    const TestCameraOperation& testCameraOperation)
{
    nx::media_db::CameraOperation result;

    result.setCameraId(testCameraOperation.id);
    result.setCameraUniqueId(testCameraOperation.camUniqueId);
    result.setCameraUniqueIdLen(testCameraOperation.uuidLen);
    result.setRecordType(nx::media_db::RecordType(testCameraOperation.code));

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
                    dataVector.emplace_back(generateFileOperation(RecordType::FileOperationAdd), false);
                    break;
                case 1:
                    dataVector.emplace_back(generateFileOperation(RecordType::FileOperationDelete), false);
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
        TestFileOperation fileOp = generateFileOperation(RecordType::FileOperationAdd);

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
            TestFileOperation fileOp = generateFileOperation(RecordType::FileOperationAdd);
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
        tfop.type = mediaFileOp.getRecordType();
        tfop.duration = mediaFileOp.getDuration();
        tfop.timeZone = mediaFileOp.getTimeZone();
        tfop.fileSize = mediaFileOp.getFileSize();
        tfop.fileIndex = mediaFileOp.getFileTypeIndex();
        tfop.startTime = mediaFileOp.getStartTime();

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

    virtual void SetUp() override
    {
        MediaServerModuleFixture::SetUp();

        workDirResource = std::make_unique<nx::ut::utils::WorkDirResource>();
        ASSERT_TRUE((bool)workDirResource->getDirName());

        const QString workDirPath = *workDirResource->getDirName();

        storage.reset(new QnFileStorageResource(&serverModule()));
        storage->setUrl(workDirPath);
        auto result = storage->initOrUpdate() == Qn::StorageInit_Ok;
        ASSERT_TRUE(result);
    }

    std::unique_ptr<QnPlatformAbstraction> platformAbstraction;

    std::unique_ptr<nx::ut::utils::WorkDirResource> workDirResource;
    QnFileStorageResourcePtr storage;
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
    {
        fopVector.push_back(
            generateFileOperation(
                i % 2 == 0 ? RecordType::FileOperationAdd : RecordType::FileOperationDelete));
    }

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
        mfop.setRecordType(tfop.type);

        ASSERT_TRUE(mfop.getCameraId() == tfop.cameraId);
        ASSERT_TRUE(mfop.getCatalog() == tfop.chunksCatalog);
        ASSERT_TRUE(mfop.getDuration() == tfop.duration);
        ASSERT_TRUE(mfop.getFileSize() == tfop.fileSize);
        ASSERT_TRUE(mfop.getFileTypeIndex() == tfop.fileIndex);
        ASSERT_TRUE(mfop.getRecordType() == tfop.type);
        ASSERT_TRUE(mfop.getStartTime() == tfop.startTime);
        ASSERT_TRUE(mfop.getTimeZone() == tfop.timeZone);
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
    ASSERT_EQ(newStartTime, mfop.getStartTime());

    mfop.setStartTime(-1);
    ASSERT_EQ(-1, mfop.getStartTime());
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

class MediaDbWriteRead: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        const auto path = *m_workDirResource.getDirName();
        m_fileName = path + "/test.nxdb";
        reopenFile();
        m_writer.setDevice(m_ioDevice.get());
        ASSERT_TRUE(media_db::MediaDbWriter::writeFileHeader(m_ioDevice.get(), 1));
        m_writer.start();
    }

    void whenSomeRecordsAreWrittenToTheDb()
    {
        for (int i = 0; i < iterations; ++i)
        {
            RecordType recordType =
                i % 2 == 0 ? RecordType::FileOperationAdd : RecordType::FileOperationDelete;
            const auto fileOp = fromTestFileOperation(generateFileOperation(recordType));

            int index = fileOp.getCameraId() * 2 + fileOp.getCatalog();
            if (recordType == RecordType::FileOperationAdd)
                m_dbData.addRecords[index].push_back(fileOp);
            else
                 m_dbData.removeRecords[index].push_back(DbReader::RemoveData
                   {fileOp.getHashInCatalog(), m_dbData.addRecords[index].size()});

            m_writer.writeRecord(fileOp);

            const auto camOp = fromTestCameraOperation(generateCameraOperation());
            m_dbData.cameras.push_back(camOp);
            m_writer.writeRecord(camOp);
        }

        m_writer.stop();
    }

    void thenAllShouldBeRetrievedCorrectly()
    {
        reopenFile();

        DbReader::Data readData;
        ASSERT_TRUE(DbReader::parse(m_ioDevice->readAll(), &readData));
        ASSERT_EQ(1, readData.header.getDbVersion());
        ASSERT_EQ(m_dbData.addRecords, readData.addRecords);
        ASSERT_EQ(m_dbData.cameras, readData.cameras);

        for (auto& catalog : readData.removeRecords)
            std::sort(catalog.second.begin(), catalog.second.end());

        ASSERT_EQ(readData.removeRecords, m_dbData.removeRecords);
    }

    void whenLastRecordIsCorrupted()
    {
    }

    void thenAllButLastRecordShouldBeRetrievedCorrectly()
    {
    }

private:
    ut::utils::WorkDirResource m_workDirResource;
    QString m_fileName;
    MediaDbWriter m_writer;
    std::unique_ptr<QFile> m_ioDevice;
    DbReader::Data m_dbData;
    const int iterations = 10;

    void reopenFile()
    {
        m_ioDevice.reset(new QFile(m_fileName));
        ASSERT_TRUE(m_ioDevice->open(QIODevice::ReadWrite));
    }

    //void checkCorrectness(size_t recordCount)
    //{
    //    reopenFile();
    //    MediaDbReader reader(m_ioDevice.get());

    //    uint8_t dbVersion;
    //    ASSERT_TRUE(reader.readFileHeader(&dbVersion));
    //    ASSERT_EQ(1, dbVersion);

    //    DBRecordQueue records;
    //    while (true)
    //    {
    //        const auto record = reader.readRecord();
    //        if (record.which() == 0)
    //            break;

    //        records.push(record);
    //    }

    //    ASSERT_EQ(m_dbRecords.size(), records.size());
    //    EqVisitor eqVisitor;

    //    for (size_t i = 0; i < m_dbRecords.size(); ++i)
    //    {
    //        const auto savedRecord = m_dbRecords.front();
    //        m_dbRecords.pop();
    //        const auto readRecord = records.front();
    //        records.pop();

    //        boost::apply_visitor(eqVisitor, savedRecord, readRecord);
    //    }
    //}
};

TEST_F(MediaDbWriteRead, correctness)
{
    whenSomeRecordsAreWrittenToTheDb();
    thenAllShouldBeRetrievedCorrectly();
}

//TEST_F(MediaDbWriteRead, corruptedFile)
//{
//    whenSomeRecordsAreWrittenToTheDb();
//    whenLastRecordIsCorrupted();
//    thenAllButLastRecordShouldBeRetrievedCorrectly();
//}

TEST_F(MediaDbTest, Migration_from_sqlite)
{
    const QString workDirPath = *workDirResource->getDirName();
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

TEST_F(MediaDbTest, StorageDB)
{
    QnStorageDb sdb(&serverModule(), storage, 1, std::chrono::seconds(600));
    auto result = sdb.open(*workDirResource->getDirName() + lit("/test.nxdb"));
    ASSERT_TRUE(result);

    sdb.loadFullFileCatalog();

    QnMutex mutex;
    std::vector<nx::utils::thread> threads;
    TestChunkManager tcm(100);

    auto writerFunc = [&mutex, &sdb, &tcm]
    {
        for (int i = 0; i < 300; ++i)
        {
            int diceRoll = nx::utils::random::number(0, 10);
            switch (diceRoll)
            {
            case 0:
            {
                std::pair<TestChunkManager::Catalog, std::deque<DeviceFileCatalog::Chunk>> p;
                QnMutexLocker lk(&mutex);
                p = tcm.generateReplaceOperation(nx::utils::random::number(10, 100));
                sdb.replaceChunks(p.first.cameraUniqueId, p.first.quality, p.second);
                break;
            }
            case 1:
            case 2:
            {
                TestChunkManager::TestChunk *chunk;
                QnMutexLocker lk(&mutex);
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
                QnMutexLocker lk(&mutex);
                chunk = tcm.generateAddOperation();
                if (!(bool)chunk)
                    break;
                sdb.addRecord(
                    chunk->catalog->cameraUniqueId,
                    chunk->catalog->quality, chunk->chunk);
                break;
            }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(7));
        }
    };

    QVector<DeviceFileCatalogPtr> dbChunkCatalogs;
    auto readerFunc = [&dbChunkCatalogs, &mutex, &tcm, &sdb]
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        for (size_t i = 0; i < 10; ++i)
        {
            sdb.loadFullFileCatalog();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    };

    for (size_t i = 0; i < 3; ++i)
        threads.push_back(nx::utils::thread(writerFunc));

    threads.push_back(nx::utils::thread(readerFunc));

    for (auto &t : threads)
        t.join();

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
        size_t notVisited = std::count_if(
                tcm.get().cbegin(),
                tcm.get().cend(),
                [](const TestChunkManager::TestChunk &tc)
                { return !tc.isDeleted && !tc.isVisited; });
        qWarning() << lit("Not visited count: %1").arg(notVisited);
    }

    ASSERT_EQ(allVisited, true);
}

TEST_F(MediaDbTest, RepaceRecord)
{
    using namespace nx::media_db;

    QnStorageDb sdb(&serverModule(), storage, 1, std::chrono::seconds(600));
    auto result = sdb.open(*workDirResource->getDirName() + lit("/test.nxdb"));
    ASSERT_TRUE(result);

    sdb.loadFullFileCatalog();

    const QString id1("1");
    const QString id2("2");
    const QString id3("3");

    DeviceFileCatalog::Chunk chunk;
    chunk.startTimeMs = 10;
    chunk.durationMs = 5;
    chunk.fileIndex = DeviceFileCatalog::Chunk::FILE_INDEX_WITH_DURATION;

    sdb.addRecord(id1, QnServer::ChunksCatalog::LowQualityCatalog, chunk);
    sdb.deleteRecords(id1, QnServer::ChunksCatalog::LowQualityCatalog, chunk.startTimeMs);
    sdb.deleteRecords(id1, QnServer::ChunksCatalog::LowQualityCatalog, chunk.startTimeMs);
    sdb.addRecord(id1, QnServer::ChunksCatalog::LowQualityCatalog, chunk);

    sdb.addRecord(id2, QnServer::ChunksCatalog::LowQualityCatalog, chunk);
    sdb.deleteRecords(id2, QnServer::ChunksCatalog::LowQualityCatalog, chunk.startTimeMs);

    sdb.addRecord(id3, QnServer::ChunksCatalog::LowQualityCatalog, chunk);
    sdb.deleteRecords(id3, QnServer::ChunksCatalog::LowQualityCatalog, chunk.startTimeMs);
    sdb.addRecord(id3, QnServer::ChunksCatalog::LowQualityCatalog, chunk);
    sdb.deleteRecords(id3, QnServer::ChunksCatalog::LowQualityCatalog, chunk.startTimeMs);

    auto dbChunkCatalogs = sdb.loadFullFileCatalog();

    ASSERT_EQ(3, dbChunkCatalogs.size());
    ASSERT_EQ(1, dbChunkCatalogs[0]->size());
    ASSERT_EQ(0, dbChunkCatalogs[1]->size());
    ASSERT_EQ(0, dbChunkCatalogs[2]->size());
}

} // namespace nx::media_db::test


//TEST_F(MediaDbTest, ReadWrite_MT)
//{
//    nx::ut::utils::WorkDirResource workDirResource;
//    ASSERT_TRUE((bool)workDirResource.getDirName());
//
//    QString fileName = QDir(*workDirResource.getDirName()).absoluteFilePath("file.mdb");
//    QFile dbFile;
//    initDbFile(&dbFile, fileName);
//
//    const size_t threadsNum = 4;
//    const size_t recordsCount = 1000;
//
//    nx::media_db::Error error;
//    TestDataManager tdm(threadsNum * recordsCount);
//    TestDbHelperHandler testHandler(&error, &tdm);
//    nx::media_db::DbHelper dbHelper(&testHandler);
//
//    dbHelper.setDevice(&dbFile);
//    dbHelper.setMode(nx::media_db::Mode::Write);
//
//    // write header explicitly
//    boost::apply_visitor(RecordWriteVisitor(&dbHelper), tdm.dataVector[0].data);
//
//    std::vector<nx::utils::thread> threads;
//    for (size_t i = 0; i < threadsNum; ++i)
//    {
//        threads.emplace_back(
//            nx::utils::thread([&dbHelper, &tdm, i, recordsCount]
//                        {
//                            size_t j = i * recordsCount + 1;
//                            for (; j < (i + 1) * recordsCount + 1; ++j)
//                                boost::apply_visitor(RecordWriteVisitor(&dbHelper),
//                                                     tdm.dataVector[j].data);
//                        }));
//    }
//
//    for (size_t i = 0; i < threadsNum; ++i)
//        if (threads[i].joinable())
//            threads[i].join();
//
//
//    dbHelper.setMode(nx::media_db::Mode::Read);
//    dbHelper.reset();
//    ASSERT_TRUE(testHandler.getErrorString() == nullptr) << *testHandler.getErrorString();
//    testHandler.resetErrorString();
//    reopenDbFile(&dbFile, fileName);
//
//    uint8_t dbVersion;
//    error = dbHelper.readFileHeader(&dbVersion);
//
//    ASSERT_TRUE(dbVersion == boost::get<TestFileHeader>(tdm.dataVector[0].data).dbVersion);
//    tdm.dataVector[0].visited = true;
//    ASSERT_TRUE(error == nx::media_db::Error::NoError);
//
//    while (error == nx::media_db::Error::NoError)
//    {
//        const auto dbRecord = dbHelper.readRecord(&error);
//        boost::apply_visitor(testHandler, dbRecord);
//    }
//
//    ASSERT_TRUE(testHandler.getErrorString() == nullptr) << *testHandler.getErrorString();
//    testHandler.resetErrorString();
//
//    size_t readRecords = std::count_if(tdm.dataVector.cbegin(), tdm.dataVector.cend(),
//                                       [](const TestData &td) { return td.visited; });
//    ASSERT_TRUE(readRecords == tdm.dataVector.size());
//
//    // Write again after we've read till the end
//    dbHelper.setMode(nx::media_db::Mode::Write);
//
//    threads.clear();
//    for (size_t i = 0; i < threadsNum; ++i)
//    {
//            threads.emplace_back(
//                nx::utils::thread([&dbHelper, &tdm, i, recordsCount]
//                            {
//                                size_t j = i * recordsCount + 1;
//                                for (; j < (i + 1) * recordsCount + 1; ++j)
//                                    boost::apply_visitor(RecordWriteVisitor(&dbHelper),
//                                                         tdm.dataVector[j].data);
//                            }));
//    }
//
//    for (size_t i = 0; i < threadsNum; ++i)
//        if (threads[i].joinable())
//            threads[i].join();
//
//    // and now read again from the beginning
//    dbHelper.setMode(nx::media_db::Mode::Read); // wait for writer here
//    reopenDbFile(&dbFile, fileName);
//
//    error = dbHelper.readFileHeader(&dbVersion);
//    ASSERT_TRUE(dbVersion == boost::get<TestFileHeader>(tdm.dataVector[0].data).dbVersion);
//    tdm.dataVector[0].visited = true;
//    ASSERT_TRUE(error == nx::media_db::Error::NoError);
//
//    size_t copySize = tdm.dataVector.size();
//    for (size_t i = 1; i < copySize; ++i)
//    {   // double test data. we should find every record
//        tdm.dataVector[i].visited = false;
//        tdm.dataVector.push_back(tdm.dataVector[i]);
//    }
//
//    size_t readCount = 0;
//    while (error == nx::media_db::Error::NoError)
//    {
//        ++readCount;
//        const auto dbRecord = dbHelper.readRecord(&error);
//        boost::apply_visitor(testHandler, dbRecord);
//    }
//
//    readRecords = std::count_if(tdm.dataVector.cbegin(), tdm.dataVector.cend(),
//                                [](const TestData &td) { return td.visited; });
//    ASSERT_TRUE(readRecords == tdm.dataVector.size()) << readRecords;
//    dbFile.close();
//    QFile::remove(fileName);
//}
//


