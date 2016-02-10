
#include "statistics_file_storage.h"

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>

#include <utils/common/model_functions.h>

namespace
{
    enum
    {
        kDefaultLimit = 128
        , kDefaultStoreDaysCount = 32
    };

    const auto localDataDirectory = QDir(QStandardPaths::writableLocation(QStandardPaths::DataLocation));

    QDir getCustomDirectory(const QString &leafDirectoryName)
    {
        auto directory = localDataDirectory;
        if (!directory.exists(leafDirectoryName))
        {
            if (!directory.mkdir(leafDirectoryName))
                return QDir();
        }

        if (!directory.cd(leafDirectoryName))
            return QDir();

        return directory;
    }

    QDir getStatisticsDirectory()
    {
        static const auto kStatisticsDirectoryName = lit("nx_statistics");
        static const auto kResult =
            getCustomDirectory(kStatisticsDirectoryName);

        return kResult;
    }

    QDir getCustomDataDirectory()
    {
        static const auto kCustomDirectoryName = lit("nx_custom");
        static const auto kResult =
            getCustomDirectory(kCustomDirectoryName);

        return kResult;
    }

    typedef std::shared_ptr<QFile> QFileCloseGuard;
    QFileCloseGuard createFileCloseGuard(QFile *file)
    {
        return QFileCloseGuard(file, [](QFile *file)
        {
            file->close();
        });
    }

    bool writeToFile(const QString &absoluteFileName
        , const QByteArray &data)
    {
        QFile output(absoluteFileName);

        output.open(QIODevice::WriteOnly);
        if (!output.isOpen())
            return false;

        qint64 writtenSize = 0;
        {
            const auto closeGuard = createFileCloseGuard(&output);
            writtenSize = output.write(data);
        }

        return (writtenSize == data.size());
    }

    QByteArray readAllFromFile(const QString &absoluteFileName)
    {
        if (!QFile::exists(absoluteFileName))
            return QByteArray();

        QFile input(absoluteFileName);
        if (!input.open(QIODevice::ReadOnly))
            return QByteArray();

        const auto closeGuard = createFileCloseGuard(&input);
        return input.readAll();
    }

    enum { kNotStatisticsFileTimeStamp = 0 };
    qint64 getStatisticsFileTimeStamp(const QString &filePath)
    {
        bool ok = 0;
        const auto result = filePath.toLongLong(&ok);
        return (ok ? result : kNotStatisticsFileTimeStamp);
    }
}

QnStatisticsFileStorage::QnStatisticsFileStorage(QObject *parent)
    : base_type(parent)

    , m_statisticsDirectory(getStatisticsDirectory())
    , m_customSettingsDirectory(getCustomDataDirectory())

    , m_limit(kDefaultLimit)
    , m_storeDaysCount(kDefaultStoreDaysCount)
{}

QnStatisticsFileStorage::~QnStatisticsFileStorage()
{}

void QnStatisticsFileStorage::storeMetrics(const QnMetricsHash &metrics)
{
    if (!m_statisticsDirectory.exists())
        return;

    const qint64 timeStampMs = QDateTime::currentMSecsSinceEpoch();

    const auto fileName = QString::number(timeStampMs);
    const auto fullFileName = m_statisticsDirectory.absoluteFilePath(fileName);

    const auto buffer = QJson::serialized(metrics);
    if (writeToFile(fullFileName, buffer))
        removeOutdatedFiles();
    else
        m_statisticsDirectory.remove(fileName);
}

QnMetricHashesList QnStatisticsFileStorage::getMetricsList(qint64 startTimeUtcMs, int limit) const
{
    if (!m_statisticsDirectory.exists())
        return QnMetricHashesList();

    // Entries are sorted by name (timestamp in our case) in descending order
    auto entries = m_statisticsDirectory.entryList(QDir::Files, QDir::Name).toStdList();
    entries.reverse();

    QnMetricHashesList result;
    for (const auto &fileName: entries)
    {
        const auto timeStamp = getStatisticsFileTimeStamp(fileName);
        if (timeStamp == kNotStatisticsFileTimeStamp)   // It is not statistics file
            continue;

        if ((timeStamp < startTimeUtcMs) || (result.size() >= limit))
            break;

        const auto fullFileName = m_statisticsDirectory.absoluteFilePath(fileName);
        const auto json = readAllFromFile(fullFileName);
        if (json.isEmpty())
            continue;

        bool success = false;
        const QnMetricsHash metrics = QJson::deserialized(
            json, QnMetricsHash(), &success);
        if (success)
            result.append(metrics);
    }
    return result;
}

bool QnStatisticsFileStorage::saveCustomSettings(const QString &name
    , const QByteArray &settings)
{
    if (!m_customSettingsDirectory.exists())
        return false;

    const auto fullFileName = m_customSettingsDirectory.absoluteFilePath(name);
    return writeToFile(fullFileName, settings);
}

QByteArray QnStatisticsFileStorage::getCustomSettings(const QString &name) const
{
    if (!m_customSettingsDirectory.exists())
        return false;

    const auto fullFileName = m_customSettingsDirectory.absoluteFilePath(name);
    return readAllFromFile(fullFileName);
}

void QnStatisticsFileStorage::removeCustomSettings(const QString &name)
{
    if (m_customSettingsDirectory.exists()
        && m_customSettingsDirectory.exists(name))
    {
        m_customSettingsDirectory.remove(name);
    }
}

void QnStatisticsFileStorage::removeOutdatedFiles()
{
    if (m_statisticsDirectory.absolutePath().isEmpty()
        || !m_statisticsDirectory.exists()
        || !m_statisticsDirectory.isReadable())
    {
        return;
    }

    // Entries are sorted by name in ascending order
    auto entries = m_statisticsDirectory.entryList(QDir::Files, QDir::Name);

    QStringList forRemoveFilesList;

    // Checks if limit of files count is exceeded
    if ((m_limit > 0) && (entries.size() > m_limit))
    {
        const int removeCount = entries.size() - m_limit;
        forRemoveFilesList = entries.mid(0, removeCount);
        entries = entries.mid(removeCount);
    }

    // Checks if some entries are outdated
    static const qint64 kMillisecondsInDay = 24 * 60 * 60 * 1000;
    const qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    const qint64 leftTimeStamp = currentTime - m_storeDaysCount * kMillisecondsInDay;
    for (const auto filePath: entries)
    {
        const qint64 timeStamp = getStatisticsFileTimeStamp(filePath);
        if (timeStamp < leftTimeStamp)
            forRemoveFilesList.append(filePath);
    }

    for(const auto &fileToRemove: forRemoveFilesList)
        m_statisticsDirectory.remove(fileToRemove);
}

