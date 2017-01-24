#include <functional>
#include <thread>
#include <algorithm>
#include <gtest/gtest.h>
#include <recorder/camera_info.h>
#include <recorder/device_file_catalog.h>

class ComposerTestHandler : public nx::caminfo::ComposerHandler
{
public:
    enum CameraPresense
    {
        CameraByIdFound,
        CameraByIdNOTFound
    };

    ComposerTestHandler(CameraPresense hasCamera = CameraByIdFound): m_hasCamera(hasCamera)
    {
        m_properties.append(QPair<QString, QString>("plainTestPropName", "plainTestPropValue"));
        m_properties.append(QPair<QString, QString>("specSymbolsTestPropName", "spec\r\n\r\nSymbolPro\npValue"));
    }

    virtual QString name() const override
    {
        return "testName";
    }

    virtual QString model() const override
    {
        return "testMod\r\nel";
    }

    virtual QString groupId() const override
    {
        return "testG\nroupId";
    }

    virtual QString groupName() const override
    {
        return "testGroupName";
    }

    virtual QString url() const override
    {
        return "testUrl";
    }

    virtual QList<QPair<QString, QString>> properties() const override
    {
        return m_properties;
    }

    virtual bool hasCamera() const override
    {
        return m_hasCamera == CameraByIdFound;
    }

    void addProperty()
    {
        m_properties.append(QPair<QString, QString>("additionalPropName", "additionalPr\r\n\ropValue"));
    }

private:
    CameraPresense m_hasCamera;
    QList<QPair<QString, QString>> m_properties;
};

class WriterTestHandler : public nx::caminfo::WriterHandler
{
public:
    WriterTestHandler() : m_needStop(false) {}

    virtual QStringList storagesUrls() const override
    {
        return m_storagesUrls;
    }

    virtual QStringList camerasIds(QnServer::ChunksCatalog) const override
    {
        return m_camerasIds;
    }

    virtual bool needStop() const override
    {
        return m_needStop;
    }

    virtual bool replaceFile(const QString& path, const QByteArray& data) override
    {
        if (m_filesWritten.contains(path))
            return false;
        m_filesWritten.insert(path, data);
        return true;
    }

    virtual nx::caminfo::ComposerHandler* composerHandler(const QString&) override
    {
        return &m_composerHandler;
    }

    const QMap<QString, QByteArray>& getWrittenData() const
    {
        return m_filesWritten;
    }

    void setComposerHandler(const ComposerTestHandler& composerHandler)
    {
        m_composerHandler = composerHandler;
    }

    void setStoragesUrls(const QStringList& storagesUrls)
    {
        m_storagesUrls = storagesUrls;
    }

    void setCamerasIds(const QStringList& camerasIds)
    {
        m_camerasIds = camerasIds;
    }

    void setNeedStop(bool value)
    {
        m_needStop = value;
    }

    void resetWrittenData()
    {
        m_filesWritten.clear();
    }

    void addProperty()
    {
        m_composerHandler.addProperty();
    }

private:
    QMap<QString, QByteArray> m_filesWritten;
    ComposerTestHandler m_composerHandler;
    QStringList m_storagesUrls;
    QStringList m_camerasIds;
    bool m_needStop;
};

const char *const kInfoFilePattern= R"#("cameraName"="testName"
"cameraModel"="testModel"
"groupId"="testGroupId"
"groupName"="testGroupName"
"cameraUrl"="testUrl"
"plainTestPropName"="plainTestPropValue"
"specSymbolsTestPropName"="specSymbolPropValue"
)#";

const char *const kInfoFileWithAdditionalPropPattern= R"#("cameraName"="testName"
"cameraModel"="testModel"
"groupId"="testGroupId"
"groupName"="testGroupName"
"cameraUrl"="testUrl"
"plainTestPropName"="plainTestPropValue"
"specSymbolsTestPropName"="specSymbolPropValue"
"additionalPropName"="additionalPropValue"
)#";

TEST(ComposerTest, CameraFound)
{
    ComposerTestHandler composerHandler(ComposerTestHandler::CameraByIdFound);
    nx::caminfo::Composer composer;

    auto result = composer.make(&composerHandler);
    ASSERT_EQ(result, QByteArray(kInfoFilePattern));
}

TEST(ComposerTest, CameraNOTFound)
{
    ComposerTestHandler composerHandler(ComposerTestHandler::CameraByIdNOTFound);
    nx::caminfo::Composer composer;

    ASSERT_TRUE(composer.make(&composerHandler).isEmpty());
}

class DerivedTestWriter : public nx::caminfo::Writer
{
public:
    using nx::caminfo::Writer::Writer;
    void setWriteInterval(std::chrono::milliseconds interval)
    {
        nx::caminfo::Writer::setWriteInterval(interval);
    }
};

class WriterTest : public ::testing::Test
{
protected:
    enum class StoragesCount
    {
        One = 1,
        Two,
        Three,
    };

    enum class CamerasCount
    {
        One = 1,
        Two,
        Three,
    };

    enum class PatternData
    {
        Original,
        WithAdditionalProp
    };

    WriterTestHandler testHandler;
    DerivedTestWriter writer;
    QStringList storagesUrls;
    QStringList camerasIds;

    WriterTest() : writer(&testHandler) {}

    void when(StoragesCount storagesCount, CamerasCount camerasCount)
    {
        storagesUrls.clear();
        camerasIds.clear();

        for (int i = 0; i < static_cast<int>(storagesCount); ++i)
            storagesUrls << (lit("storage") + QString::number(i));

        for (int i = 0; i < static_cast<int>(camerasCount); ++i)
            camerasIds << (lit("camera") + QString::number(i));

        testHandler.setStoragesUrls(storagesUrls);
        testHandler.setCamerasIds(camerasIds);
    }

    void thenWrittenFiles(int filesCount)
    {
        ASSERT_EQ(filesCount, testHandler.getWrittenData().size());
    }

    void thenAllPossiblePathAndCameraIdsCombinationsExist()
    {
        const auto& writtenData = testHandler.getWrittenData();
        for (const auto& storageUrl: storagesUrls)
            for (int i = 0; i < QnServer::ChunksCatalogCount; ++i)
                for (const auto& cameraId: camerasIds)
                {
                    bool pathFound = false;
                    for (auto it = writtenData.constBegin(); it != writtenData.constEnd(); ++it)
                        if (it.key().contains(storageUrl)
                            && it.key().contains(DeviceFileCatalog::prefixByCatalog(static_cast<QnServer::ChunksCatalog>(i)))
                            && it.key().contains(cameraId))
                        {
                            pathFound = true;
                            break;
                        }
                    ASSERT_TRUE(pathFound);
                }
    }

    void thenAllFilesContainsPatternData(PatternData patternData)
    {
        const auto& writtenData = testHandler.getWrittenData();
        for (auto it = writtenData.constBegin(); it != writtenData.constEnd(); ++it)
            ASSERT_EQ(it.value(), QByteArray(patternData == PatternData::Original ?
                                                 kInfoFilePattern:
                                                 kInfoFileWithAdditionalPropPattern));
    }

    void resetWrittenData()
    {
        testHandler.resetWrittenData();
    }

    void addCameraProperty()
    {
        testHandler.addProperty();
    }
};

TEST_F(WriterTest, AllFilesWritten)
{
    when(StoragesCount::Two, CamerasCount::Three);

    writer.write();

    thenWrittenFiles(12);
    thenAllPossiblePathAndCameraIdsCombinationsExist();
    thenAllFilesContainsPatternData(PatternData::Original);
}

TEST_F(WriterTest, DontWriteSameData)
{
    auto kWriteInterval = std::chrono::milliseconds(10);
    writer.setWriteInterval(kWriteInterval);
    when(StoragesCount::Two, CamerasCount::Three);

    writer.write();

    thenWrittenFiles(12);
    resetWrittenData();
    thenWrittenFiles(0);

    std::this_thread::sleep_for(kWriteInterval + std::chrono::milliseconds(5));
    writer.write();
    thenWrittenFiles(0);
}

TEST_F(WriterTest, IntervalTest)
{
    auto kWriteInterval = std::chrono::milliseconds(100);
    writer.setWriteInterval(kWriteInterval);
    when(StoragesCount::Two, CamerasCount::Three);

    writer.write();

    thenWrittenFiles(12);
    resetWrittenData();
    thenWrittenFiles(0);

    addCameraProperty();
    std::this_thread::sleep_for(kWriteInterval/2);
    writer.write();
    thenWrittenFiles(0);

    std::this_thread::sleep_for(kWriteInterval/2 + std::chrono::milliseconds(5));
    writer.write();
    thenWrittenFiles(12);
    thenAllFilesContainsPatternData(PatternData::WithAdditionalProp);
}


const QnUuid kModuleGuid = QnUuid::fromStringSafe(lit("{95a380a7-f7d7-4a4a-906f-b9c101a33ef1}"));
const QnUuid kArchiveCamTypeGuid = QnUuid::fromStringSafe(lit("{1d250fa0-ce88-4358-9bc9-4f39d0ce3b12}"));

QString kCameraId("cameraId");
QString kFilePath("/some/" + kCameraId);

const std::array<const char*, 5> kInvalidFileDataList = {
R"#("cameraName"="testName"
"cameraModel"="testModel"
"groupId"="testGroupId"
"groupName"="testGroupName
"cameraUrl"="testUrl"
"plainTestPropName"="plainTestPropValue"
"specSymbolsTestPropName"="specSymbolPropValue"
"additionalPropName"="additionalPropValue"
)#",

R"#("cameraName"="testName"
"cameraModel"="testModel"
"groupId"="testGroupId"
"groupName"="testGroupName"
"cameraUrl"="testUrl"
"plainTestPropName""plainTestPropValue"
"specSymbolsTestPropName"="specSymbolPropValue"
"additionalPropName"="additionalPropValue"
)#",

R"#("cameraName"="testName"
"cameraModel"="testModel"
"testGroupId"
"groupName"="testGroupName"
"cameraUrl"="testUrl"
"plainTestPropName"="plainTestPropValue"
"specSymbolsTestPropName"="specSymbolPropValue"
"additionalPropName"="additionalPropValue"
)#",

R"#("cameraName"="testName"
="testModel"
"groupId"="testGroupId"
"groupName"="testGroupName"
"cameraUrl"="testUrl"
"plainTestPropName"="plainTestPropValue"
"specSymbolsTestPropName"="specSymbolPropValue"
"additionalPropName"="additionalPropValue"
)#",

R"#("cameraName"="testName"
"cameraModel"="testModel"
"groupId"="testGroupId"
"groupName"="testGroupName"
"cameraUrl"=
"plainTestPropName"="plainTestPropValue"
"specSymbolsTestPropName"="specSymbolPropValue"
"additionalPropName"="additionalPropValue"
)#",

};

enum class CameraPresence
{
    InTheResourcePool,
    NotInTheResourcePool
};

enum class ModuleGuid
{
    Found,
    NotFound
};

enum class ArchiveCamTypeId
{
    Found,
    NotFound
};

enum class GetFileData
{
    Successfull,
    Failed
};

enum class ResultIs
{
    Empty,
    NotEmpty
};

class ReaderTestHandler: public nx::caminfo::ReaderHandler
{
public:
    QnUuid moduleGuid() const override
    {
        return moduleGuidFlag == ModuleGuid::Found ? kModuleGuid : QnUuid();
    }

    QnUuid archiveCamTypeId() const override
    {
        return archiveCamTypeIdFlag == ArchiveCamTypeId::Found ? kArchiveCamTypeGuid : QnUuid();
    }

    bool isCameraInResPool(const QnUuid&) const override
    {
        return camInRpFlag == CameraPresence::InTheResourcePool;
    }

    void handleError(const QString& message) const override
    {
        errMsg = message;
    }

    CameraPresence camInRpFlag;
    ModuleGuid moduleGuidFlag;
    ArchiveCamTypeId archiveCamTypeIdFlag;

    mutable QString errMsg;
};

class ReaderTest: public ::testing::Test
{
protected:
    ReaderTest() :
        reader(&readerHandler),
        fileInfo(QnAbstractStorageResource::FileInfo(kFilePath, 1))
    {}

    void when(CameraPresence presenceFlag,
              ModuleGuid moduleGuidFlag,
              ArchiveCamTypeId archiveCamTypeIdFlag,
              GetFileData getFileDataFlag)
    {
        readerHandler.camInRpFlag = presenceFlag;
        readerHandler.moduleGuidFlag = moduleGuidFlag;
        readerHandler.archiveCamTypeIdFlag = archiveCamTypeIdFlag;

        if (getFileDataFlag == GetFileData::Successfull)
            getDataFunc = [](const QString&) { return QByteArray(kInfoFileWithAdditionalPropPattern); };
        else
            getDataFunc = [](const QString&) { return QByteArray(); };
    }

    void then(ResultIs resultIsFlag)
    {
        if (resultIsFlag == ResultIs::Empty)
        {
            ASSERT_EQ(camDataList.size(), 0);
            ASSERT_FALSE(readerHandler.errMsg.isEmpty());
        }
        else
        {
            ASSERT_EQ(camDataList.size(), 1);
        }
        readerHandler.errMsg.clear();
    }

    ReaderTestHandler readerHandler;
    nx::caminfo::Reader reader;
    std::function<QByteArray(const QString&)> getDataFunc;
    QnAbstractStorageResource::FileInfo fileInfo;
    nx::caminfo::ArchiveCameraDataList camDataList;
};

TEST_F(ReaderTest, HandlerError_moduleGuid)
{
    when(CameraPresence::NotInTheResourcePool,
         ModuleGuid::NotFound,
         ArchiveCamTypeId::Found,
         GetFileData::Successfull);
    reader.loadCameraInfo(fileInfo, camDataList, getDataFunc);
    then(ResultIs::Empty);
}

TEST_F(ReaderTest, HandlerError_cameraIsInResPool)
{
    when(CameraPresence::InTheResourcePool,
         ModuleGuid::Found,
         ArchiveCamTypeId::Found,
         GetFileData::Successfull);
    reader.loadCameraInfo(fileInfo, camDataList, getDataFunc);
    then(ResultIs::Empty);
}

TEST_F(ReaderTest, HandlerError_typeIdNotFound)
{
    when(CameraPresence::NotInTheResourcePool,
         ModuleGuid::Found,
         ArchiveCamTypeId::NotFound,
         GetFileData::Successfull);
    reader.loadCameraInfo(fileInfo, camDataList, getDataFunc);
    then(ResultIs::Empty);
}

TEST_F(ReaderTest, HandlerError_getFileDataError)
{
    when(CameraPresence::NotInTheResourcePool,
         ModuleGuid::Found,
         ArchiveCamTypeId::Found,
         GetFileData::Failed);
    reader.loadCameraInfo(fileInfo, camDataList, getDataFunc);
    then(ResultIs::Empty);
}

TEST_F(ReaderTest, ErrorsInData)
{
    when(CameraPresence::NotInTheResourcePool,
         ModuleGuid::Found,
         ArchiveCamTypeId::Found,
         GetFileData::Successfull);
    for (const char* fileData: kInvalidFileDataList)
    {
        reader.loadCameraInfo(
            fileInfo,
            camDataList,
            [fileData](const QString&)
            {
                return QByteArray(fileData);
            });
        then(ResultIs::Empty);
    }
}
