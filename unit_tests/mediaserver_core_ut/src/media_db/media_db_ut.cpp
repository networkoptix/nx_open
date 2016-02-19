#define GTEST_HAS_TR1_TUPLE     0
#define GTEST_USE_OWN_TR1_TUPLE 1

#define GTEST_HAS_POSIX_RE 0
#include <gtest/gtest.h>

#include <random>
#include <climits>

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

TestFileOperation generateFileOperation()
{
    return{ genRandomNumber<0, LLONG_MAX>(), genRandomNumber<0, LLONG_MAX>(),
            (int)genRandomNumber<0, 1>(), (int)genRandomNumber<0, 65535>(),
            (int)genRandomNumber<0, 1048575>(), (int)genRandomNumber<0, 1>() };
}

struct TestCameraOperation
{
    int code;
    int uuidLen;
    int id;
};

TestCameraOperation generateCameraOperaion()
{
    return{ 2, (int)genRandomNumber<0, 16383>(), (int)genRandomNumber<0, 65535>() };
}

TEST(MediaDb_test, ReadWrite)
{
    QString workDirPath = qApp->applicationDirPath() + lit("/tmp/media_db");
    QDir().mkpath(workDirPath);

    QFile dbFile(workDirPath + lit("/file.mdb"));
    dbFile.open(QIODevice::ReadOnly);
    TestDbHelperHandler testHandler;
    nx::media_db::DbHelper dbHelper(&dbFile, &testHandler);

    recursiveClean(workDirPath);
}