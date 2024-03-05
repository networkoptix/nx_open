// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "protocol_handler.h"

#include <QtCore/QStandardPaths>
#include <QtCore/QDir>
#include <QtCore/QSettings>
#include <QtCore/QTemporaryFile>

#include <nx/utils/log/log.h>
#include <nx/utils/literal.h>

#include <nx/vms/utils/desktop_file_linux.h>

namespace {

struct ProtocolHandlerFunctionsTag{};
static const nx::log::Tag kLogTag(typeid(ProtocolHandlerFunctionsTag));

const auto kAssociationsSection = QByteArray("Added Associations");

bool registerMimeType(const QString& protocol)
{
    const auto kSchemeStringStart = QByteArray("x-scheme-handler/") + protocol.toLatin1();

    const auto appsLocation = QStandardPaths::writableLocation(
        QStandardPaths::ApplicationsLocation);
    if (appsLocation.isEmpty())
        return false;

    enum class State
    {
        SearchSection,
        SearchSectionEnd,
        WritingRestOfFile,
        Finished
    } state = State::SearchSection;

    const QString mimeAppsListFileName = QDir(appsLocation).absoluteFilePath("mimeapps.list");

    QTemporaryFile outFile;
    if (!outFile.open())
        return false;

    QFile mimeAppsListFile(mimeAppsListFileName);
    qint64 emptyLinePos = -1; //< It is for trimming trailing empty lines from the section.

    auto writeScheme = [&kSchemeStringStart, &protocol, &outFile, &emptyLinePos]()
        {
            if (emptyLinePos >= 0)
            {
                outFile.seek(emptyLinePos);
                outFile.resize(emptyLinePos);
            }
            outFile.write(kSchemeStringStart + "=" + protocol.toLatin1() + ".desktop\n");
        };

    const auto kSectionStart = '[' + kAssociationsSection + ']';

    if (!mimeAppsListFile.open(QFile::ReadOnly))
    {
        outFile.write(kSectionStart + '\n');
        writeScheme();
        state = State::Finished;
    }

    while (state != State::Finished)
    {
        switch (state)
        {
            case State::SearchSection:
                if (mimeAppsListFile.atEnd())
                {
                    outFile.write(kSectionStart + '\n');
                    writeScheme();

                    state = State::Finished;
                }
                else
                {
                    const auto line = mimeAppsListFile.readLine();
                    outFile.write(line);

                    if (line.contains(kSectionStart))
                        state = State::SearchSectionEnd;
                }
                break;

            case State::SearchSectionEnd:
                if (mimeAppsListFile.atEnd())
                {
                    writeScheme();
                    state = State::Finished;
                }
                else
                {
                    const auto line = mimeAppsListFile.readLine();
                    const auto trimmed = line.trimmed();

                    if (trimmed.isEmpty())
                    {
                        if (emptyLinePos < 0)
                            emptyLinePos = outFile.pos();
                    }
                    else
                    {
                        if (line.contains(kSchemeStringStart))
                        {
                            state = State::WritingRestOfFile;
                        }
                        else if (trimmed.startsWith('['))
                        {
                            writeScheme();
                            outFile.write("\n");
                            state = State::WritingRestOfFile;
                        }

                        emptyLinePos = -1;
                    }

                    outFile.write(line);
                }

                break;

            case State::WritingRestOfFile:
                if (mimeAppsListFile.atEnd())
                    state = State::Finished;
                else
                    outFile.write(mimeAppsListFile.readLine());
                break;

            case State::Finished:
                break;
        }
    }

    mimeAppsListFile.close();
    outFile.seek(0);

    if (!mimeAppsListFile.open(QFile::WriteOnly))
        return false;

    do
    {
        mimeAppsListFile.write(outFile.readLine());
    } while (!outFile.atEnd());

    outFile.close();
    mimeAppsListFile.close();

    return true;
}

} // namespace

namespace nx {
namespace vms {
namespace utils {

using nx::utils::SoftwareVersion;

RegisterSystemUriProtocolHandlerResult registerSystemUriProtocolHandler(
    const QString& protocol,
    const QString& applicationBinaryPath,
    const QString& applicationName,
    const QString& description,
    const QString& /*customization*/,
    const SoftwareVersion& version)
{
    RegisterSystemUriProtocolHandlerResult result;

    static const nx::log::Tag kTag(QLatin1String("registerSystemUriProtocolHandler"));

    const auto appsLocation = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    if (appsLocation.isEmpty())
        return result;

    const QString handlerFile(QDir(appsLocation).filePath(protocol + ".desktop"));

    bool updateDesktopFile = true;

    if (QFile::exists(handlerFile))
    {
        QScopedPointer<QSettings> settings(new QSettings(handlerFile, QSettings::IniFormat));
        settings->beginGroup("Desktop Entry");
        result.previousVersion = SoftwareVersion(settings->value("Version").toString());
        if (result.previousVersion > version)
        {
            updateDesktopFile = false;
        }
        else if (result.previousVersion == version &&
            settings->value("Exec").toString() == applicationBinaryPath)
        {
            updateDesktopFile = false;
        }
    }

    if (updateDesktopFile)
    {
        if (!createDesktopFile(
            handlerFile,
            applicationBinaryPath,
            applicationName,
            description,
            iconFileName(),
            version,
            protocol))
        {
            return result;
        }
    }

    NX_INFO(kLogTag, nx::format("Scheme handler file updated: %1").arg(handlerFile));

    if (!registerMimeType(protocol))
        return result;

    NX_INFO(kLogTag, nx::format("MimeType %1 has been registered").arg(protocol));

    result.success = true;
    return result;
}

} // namespace utils
} // namespace vms
} // namespace nx
