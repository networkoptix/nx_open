/**********************************************************
* Sep 9, 2015
* a.kolesnikov
***********************************************************/

#include "settings.h"

#include "command_line_parser.h"


QnSettings::QnSettings(
    QSettings::Scope scope,
    const QString& organization,
    const QString& application)
:
    m_systemSettings(
        scope,
        organization,
        application)
{
}

QnSettings::QnSettings(
    const QString& fileName,
    QSettings::Format format)
:
    m_systemSettings(
        fileName,
        format)
{
}

void QnSettings::parseArgs(int argc, char **argv)
{
    parseCmdArgs(argc, argv, &m_args);
}

QVariant QnSettings::value(
    const QString& key,
    const QVariant& defaultValue) const
{
    auto argIter = m_args.find(key);
    if (argIter != m_args.end())
        return QVariant(argIter->second);

    return m_systemSettings.value(key, defaultValue);
}
