// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <cstring>

#include <QtCore/QByteArray>
#include <QtCore/QDateTime>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>

#include <nx/vms/metadata/metadata_archive.h>

namespace nx::vms::metadata::test {

namespace {

static const int kBaseRecordSize = 64;
static const int kAggregationIntervalSeconds = 1;

/** In-memory index file. The reported size may be set apart from the actual data. */
class BinaryFileStub: public AbstractMetadataBinaryFile
{
public:
    BinaryFileStub(const QByteArray& data): m_data(data), m_reportedSize(data.size()) {}

    void setReportedSize(int64_t size) { m_reportedSize = size; }

    virtual void close() override { m_isOpen = false; }
    virtual void setFileName(const QString& name) override { m_fileName = name; }
    virtual bool openReadOnly() override { return m_isOpen = true; }
    virtual bool openRW() override { return m_isOpen = true; }
    virtual bool resize(int64_t /*size*/) override { return false; }
    virtual bool seek(int64_t offset) override
    {
        m_position = offset;
        return true;
    }
    virtual QString fileName() const override { return m_fileName; }
    virtual bool isOpen() const override { return m_isOpen; }
    virtual int64_t size() const override { return m_reportedSize; }
    virtual int64_t write(const char* /*buffer*/, qint64 /*count*/) override { return -1; }
    virtual bool flush() override { return true; }

    virtual int64_t read(char* data, int64_t maxSize) override
    {
        const int64_t result = std::min(maxSize, (int64_t) m_data.size() - m_position);
        if (result <= 0)
            return 0;
        memcpy(data, m_data.constData() + m_position, result);
        m_position += result;
        return result;
    }

private:
    QByteArray m_data;
    int64_t m_reportedSize = 0;
    int64_t m_position = 0;
    bool m_isOpen = false;
    QString m_fileName = "/nx_unit_test/nonexistent_index_file";
};

QByteArray makeIndexFile(int recordCount, qint16 intervalMs = 1000)
{
    IndexHeader header;
    header.startTimeMs = 1000000;
    header.intervalMs = intervalMs;
    header.version = 4;
    header.recordSize = kBaseRecordSize;
    header.wordSize = 0;
    memset(header.dummy, 0, sizeof(header.dummy));

    QByteArray result((const char*) &header, sizeof(header));
    for (int i = 0; i < recordCount; ++i)
    {
        IndexRecord record{};
        record.start = i * intervalMs;
        record.duration = intervalMs;
        result.append((const char*) &record, sizeof(record));
    }
    return result;
}

class MetadataArchiveIndex: public ::testing::Test
{
protected:
    MetadataArchive archive{"motion",
        kBaseRecordSize,
        /*wordSize*/ 0,
        kAggregationIntervalSeconds,
        /*dataDir*/ QString(),
        /*physicalId*/ "index test",
        /*channel*/ 0};

    Index index{&archive};
};

/** Gives the test access to the file handling of the archive. */
class ArchiveStub: public MetadataArchive
{
public:
    using MetadataArchive::MetadataArchive;
    using MetadataArchive::fillFileNames;
    using MetadataArchive::openFiles;
};

} // namespace

TEST_F(MetadataArchiveIndex, validFileIsLoaded)
{
    BinaryFileStub file(makeIndexFile(3));
    ASSERT_TRUE(index.load(file));
    ASSERT_EQ(3U, index.records.size());
    ASSERT_EQ(1000000, index.header.startTimeMs);
    ASSERT_EQ(2000U, index.records.back().start);
}

TEST_F(MetadataArchiveIndex, emptyFileIsLoaded)
{
    BinaryFileStub file(makeIndexFile(0));
    ASSERT_TRUE(index.load(file));
    ASSERT_TRUE(index.records.empty());
}

TEST_F(MetadataArchiveIndex, fileWithTrailingBytesIsRejected)
{
    BinaryFileStub file(makeIndexFile(3) + QByteArray(3, 'x'));
    ASSERT_FALSE(index.load(file));
    ASSERT_TRUE(index.records.empty());
}

TEST_F(MetadataArchiveIndex, truncatedFileIsRejected)
{
    BinaryFileStub file(makeIndexFile(3));
    file.setReportedSize(sizeof(IndexHeader) + 5 * sizeof(IndexRecord));
    ASSERT_FALSE(index.load(file));
    ASSERT_TRUE(index.records.empty());
}

// A corrupted file size must not be turned into an allocation of the same size: VMS-61728.
TEST_F(MetadataArchiveIndex, unreasonableRecordCountIsRejected)
{
    BinaryFileStub file(makeIndexFile(3));
    file.setReportedSize(sizeof(IndexHeader) + 200LL * 1024 * 1024 * 1024 * sizeof(IndexRecord));
    ASSERT_FALSE(index.load(file));
    ASSERT_TRUE(index.records.empty());
}

// load() removes a corrupt index file, so the month has to be started over: VMS-61728.
TEST(MetadataArchiveFiles, corruptIndexIsRecreated)
{
    QTemporaryDir dataDir;
    ASSERT_TRUE(dataDir.isValid());

    const qint64 timestampMs = QDateTime(QDate(2026, 5, 10).startOfDay()).toMSecsSinceEpoch();
    ArchiveStub archive{"motion",
        kBaseRecordSize,
        /*wordSize*/ 0,
        kAggregationIntervalSeconds,
        dataDir.path(),
        /*physicalId*/ "index test",
        /*channel*/ 0};

    ASSERT_TRUE(archive.openFiles(timestampMs));

    auto indexFile = MetadataBinaryFileFactory::instance().create();
    archive.fillFileNames(timestampMs, indexFile.get());
    const QString indexFileName = indexFile->fileName();
    indexFile.reset();

    // Append an incomplete record to make the file corrupt.
    {
        QFile file(indexFileName);
        ASSERT_TRUE(file.open(QFile::Append));
        ASSERT_EQ(3, file.write("abc", 3));
    }

    ASSERT_TRUE(archive.openFiles(timestampMs));

    QFile file(indexFileName);
    ASSERT_TRUE(file.open(QFile::ReadOnly));
    ASSERT_EQ((qint64) sizeof(IndexHeader), file.size());

    IndexHeader header;
    ASSERT_EQ((qint64) sizeof(header), file.read((char*) &header, sizeof(header)));
    ASSERT_EQ(QDateTime(QDate(2026, 5, 1).startOfDay()).toMSecsSinceEpoch(), header.startTimeMs);
}

} // namespace nx::vms::metadata::test
