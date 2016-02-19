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

class TestDbHelperHandler : public nx::media_db::DbHelperHandler
{
public:
    void handleCameraOp(const nx::media_db::CameraOperation &cameraOp, 
                        nx::media_db::Error error) override
    {
    }

    void handleMediaFileOp(const nx::media_db::MediaFileOperation &mediaFileOp, 
                           nx::media_db::Error error) override
    {
    }

    void handleError(nx::media_db::Error error) override
    {
    }

    void handleRecordWrite(nx::media_db::Error error) override
    {
    }
private:
};

template<qint64 From, qint64 To>
qint64 genRandomNumber()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(From, To);

    return dist(gen);
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
};

TestCameraOperation generateCameraOperation()
{
    return{ 2, (int)genRandomNumber<0, 16383>(), (int)genRandomNumber<0, 65535>() };
}

typedef boost::variant<TestFileHeader, TestFileOperation, TestCameraOperation> TestRecord;

template<typename Record>
class RecordTestVisitor: public boost::static_visitor<bool>
{
public:
    RecordTestVisitor(const Record &r) : m_record(r) {}

    template<typename OtherRecord>
    typename std::enable_if<std::is_same<Record, OtherRecord>::value, bool>::type 
    operator() (const OtherRecord &other) const
    {
        return other == m_record;
    }

    template<typename OtherRecord>
    typename std::enable_if<!std::is_same<Record, OtherRecord>::value, bool>::type 
    operator() (const OtherRecord &other) const
    {
        return false;
    }
private:
    Record m_record;
};

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
        fileOp.setCameraId(tfo.duration);
        fileOp.setFileSize(tfo.fileSize);
        fileOp.setRecordType(nx::media_db::RecordType(tfo.code));
        fileOp.setCameraId(tfo.startTime);

        m_helper->writeRecordAsync(fileOp);
    }

    void operator() (const TestCameraOperation &tco) const
    {
        nx::media_db::CameraOperation camOp;
        camOp.setCameraId(tco.code);
        camOp.setCameraId(tco.uuidLen);
        camOp.setRecordType(nx::media_db::RecordType(tco.code));

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
        for (size_t i = 0; i < dataSize; ++i) {
            switch (genRandomNumber<0, 2>()) {
            case 0:
                dataVector.emplace_back(generateFileHeader(), false);
                break;
            case 1:
                switch (genRandomNumber<0, 1>()) {
                case 0:
                    dataVector.emplace_back(generateFileOperation(0), false);
                    break;
                case 1:
                    dataVector.emplace_back(generateFileOperation(1), false);
                    break;
                }
                break;
            case 2:
                dataVector.emplace_back(generateCameraOperation(), false);
                break;
            }
        }
    }

    template<typename Record>
    bool seekAndSet(const Record &record)
    {
        auto it = std::find_if(dataVector.begin(), dataVector.end(),
                               [&record](const TestData &td) {
                                    return boost::apply_visitor(
                                                record, 
                                                RecordTestVisitor<Record>(td.data));
                               });
        if (it != m_dataVector.end())
            it->visited = true;
    }

    TestDataVector dataVector;
};

TEST(MediaDb_test, ReadWrite)
{
    QString workDirPath = qApp->applicationDirPath() + lit("/tmp/media_db");
    QDir().mkpath(workDirPath);

    QFile dbFile(workDirPath + lit("/file.mdb"));
    dbFile.open(QIODevice::ReadOnly);
    TestDbHelperHandler testHandler;
    nx::media_db::DbHelper dbHelper(&dbFile, &testHandler);

    TestDataManager tdm(10000);
    for (auto &data : tdm.dataVector)
        boost::apply_visitor(RecordWriteVisitor(&dbHelper), data.data);

    recursiveClean(workDirPath);
}