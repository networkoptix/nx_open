#include <functional>
#include <thread>
#include <algorithm>
#include <array>
#include <gtest/gtest.h>
#include <recorder/camera_info.h>
#include <recorder/device_file_catalog.h>

class ComposerTestHandler : public nx::caminfo::ComposerHandler
{
public:
    ComposerTestHandler()
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

    void addProperty()
    {
        m_properties.append(QPair<QString, QString>("additionalPropName", "additionalPr\r\n\ropValue"));
    }

private:
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

    virtual bool handleFileData(const QString& path, const QByteArray& data) override
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

    void setHasCamera(bool value)
    {
        m_hasCamera = value;
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
    bool m_hasCamera;
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
    ComposerTestHandler composerHandler;
    nx::caminfo::Composer composer;

    auto result = composer.make(&composerHandler);
    ASSERT_EQ(result, QByteArray(kInfoFilePattern));
}

TEST(ComposerTest, CameraNOTFound)
{
    nx::caminfo::Composer composer;

    ASSERT_TRUE(composer.make(nullptr).isEmpty());
}

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
    nx::caminfo::Writer writer;
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
    when(StoragesCount::Two, CamerasCount::Three);

    writer.write();

    thenWrittenFiles(12);
    resetWrittenData();
    thenWrittenFiles(0);

    writer.write();
    thenWrittenFiles(0);
}

const QnUuid kModuleGuid = QnUuid::fromStringSafe(lit("{95a380a7-f7d7-4a4a-906f-b9c101a33ef1}"));
const QnUuid kArchiveCamTypeGuid = QnUuid::fromStringSafe(lit("{1d250fa0-ce88-4358-9bc9-4f39d0ce3b12}"));

QString kCameraId("cameraId");
QString kFilePath("/some/" + kCameraId);

struct ExpectedInResult
{
    QString line;
    bool shouldBeParsed;
    QString key;
    QString value;

    ExpectedInResult(const QString& line, bool shouldBeParsed, const QString& key = "", const QString& value= ""):
        line(line),
        shouldBeParsed(shouldBeParsed),
        key(key),
        value(value)
    {}
};

static const QList<ExpectedInResult> kPartiallyInvalidPropertyList = {
    ExpectedInResult(R"#(_$garbage_!"validKey"="testName"_$garbage_!)#", true, "validKey", "testName"),
    ExpectedInResult(R"#("groupId""testGroupId")#", false),
    ExpectedInResult(R"#("groupName"="testGroupName)#", false),
    ExpectedInResult(R"#("validJsonObj"="{"a": 1}"_$garbage_!)#", true, "validJsonObj", R"#({"a": 1})#"),
    ExpectedInResult(R"#("validJsonArray"="["a", 1]"_$garbage_!)#", true, "validJsonArray", R"#(["a", 1])#"),
    ExpectedInResult(R"#("{}")#", false),
    ExpectedInResult(R"#("[]")#", false),
};

static const QByteArray infoFileFromExpectedList(const QList<ExpectedInResult>& expected)
{
    QByteArray result;
    for (const auto& entry: expected)
        result += entry.line + '\n';
    return result;
}

static QList<ExpectedInResult> filterValid(const QList<ExpectedInResult>& expected)
{
    QList<ExpectedInResult> result;
    std::copy_if(
        expected.cbegin(), expected.cend(), std::back_inserter(result),
        [](const ExpectedInResult& entry) { return entry.shouldBeParsed; });
    return result;
}

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

    void handleError(const nx::caminfo::ReaderErrorInfo& errorInfo) const override
    {
        errMsg = errorInfo.message;
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

        getDataFunc2 = [](const QString&) { return QByteArray(kInfoFilePattern); };
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

    void thenDataIsCorrect()
    {
        const nx::caminfo::ArchiveCameraData& camData = camDataList[0];
        const nx::vms::api::CameraData& coreData = camData.coreData;

        ASSERT_EQ(coreData.groupId, "testGroupId");
        ASSERT_EQ(coreData.groupName, "testGroupName");
        ASSERT_EQ(coreData.id, guidFromArbitraryData(lit("cameraId")));
        ASSERT_EQ(coreData.mac, QnLatin1Array());
        ASSERT_EQ(coreData.model, "testModel");
        ASSERT_EQ(coreData.name, "testName");
        ASSERT_EQ(coreData.parentId, kModuleGuid);
        ASSERT_EQ(coreData.physicalId, fileInfo.fileName());
        ASSERT_EQ(coreData.typeId, kArchiveCamTypeGuid);
        ASSERT_EQ(coreData.url, "testUrl");
    }


    ReaderTestHandler readerHandler;
    std::function<QByteArray(const QString&)> getDataFunc;
    std::function<QByteArray(const QString&)> getDataFunc2;
    QnAbstractStorageResource::FileInfo fileInfo;
    nx::caminfo::ArchiveCameraDataList camDataList;
};

TEST_F(ReaderTest, HandlerError_moduleGuid)
{
    when(CameraPresence::NotInTheResourcePool,
         ModuleGuid::NotFound,
         ArchiveCamTypeId::Found,
         GetFileData::Successfull);
    nx::caminfo::Reader(&readerHandler, fileInfo, getDataFunc)(&camDataList);
    then(ResultIs::Empty);
}

TEST_F(ReaderTest, HandlerError_cameraIsInResPool)
{
    when(CameraPresence::InTheResourcePool,
         ModuleGuid::Found,
         ArchiveCamTypeId::Found,
         GetFileData::Successfull);
    nx::caminfo::Reader(&readerHandler, fileInfo, getDataFunc)(&camDataList);
    then(ResultIs::Empty);
}

TEST_F(ReaderTest, HandlerError_typeIdNotFound)
{
    when(CameraPresence::NotInTheResourcePool,
         ModuleGuid::Found,
         ArchiveCamTypeId::NotFound,
         GetFileData::Successfull);
    nx::caminfo::Reader(&readerHandler, fileInfo, getDataFunc)(&camDataList);
    then(ResultIs::Empty);
}

TEST_F(ReaderTest, HandlerError_getFileDataError)
{
    when(CameraPresence::NotInTheResourcePool,
         ModuleGuid::Found,
         ArchiveCamTypeId::Found,
         GetFileData::Failed);
    nx::caminfo::Reader(&readerHandler, fileInfo, getDataFunc)(&camDataList);
    then(ResultIs::Empty);
}

TEST_F(ReaderTest, ErrorsInData)
{
    when(CameraPresence::NotInTheResourcePool,
         ModuleGuid::Found,
         ArchiveCamTypeId::Found,
         GetFileData::Successfull);

    nx::caminfo::Reader(&readerHandler, fileInfo,
        [](const QString&) { return infoFileFromExpectedList(kPartiallyInvalidPropertyList); })(
            &camDataList);

    const auto cameraData = camDataList[0];
    const auto expectedValidCount = std::count_if(
        kPartiallyInvalidPropertyList.cbegin(),
        kPartiallyInvalidPropertyList.cend(),
        [](const auto& entry) { return entry.shouldBeParsed; });
    ASSERT_EQ(expectedValidCount, cameraData.properties.size());

    for (const auto& expectedEntry: filterValid(kPartiallyInvalidPropertyList))
    {
        const auto expectedPropertyIt = std::find_if(
            cameraData.properties.cbegin(), cameraData.properties.cend(),
            [&expectedEntry](const auto& property)
            {
                return property.name == expectedEntry.key && property.value == expectedEntry.value;
            });
        ASSERT_NE(cameraData.properties.cend(), expectedPropertyIt);
    }
}

TEST_F(ReaderTest, CorrectData)
{
    when(CameraPresence::NotInTheResourcePool,
         ModuleGuid::Found,
         ArchiveCamTypeId::Found,
         GetFileData::Successfull);
    nx::caminfo::Reader(&readerHandler, fileInfo, getDataFunc)(&camDataList);
    then(ResultIs::NotEmpty);
    thenDataIsCorrect();
}

TEST_F(ReaderTest, CorrectDataMultiple)
{
    when(CameraPresence::NotInTheResourcePool,
         ModuleGuid::Found,
         ArchiveCamTypeId::Found,
         GetFileData::Successfull);

    nx::caminfo::Reader(&readerHandler, fileInfo, getDataFunc2)(&camDataList);
    ASSERT_EQ(camDataList[0].properties.size(), 2);

    camDataList.clear();
    nx::caminfo::Reader(&readerHandler, fileInfo, getDataFunc)(&camDataList);
    ASSERT_EQ(camDataList[0].properties.size(), 3);

    camDataList.clear();
    nx::caminfo::Reader(&readerHandler, fileInfo, getDataFunc2)(&camDataList);
    ASSERT_EQ(camDataList[0].properties.size(), 2);
}
