/**********************************************************
* Sep 9, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_SETTINGS_H
#define NX_SETTINGS_H

#include <map>

#include <QtCore/QSettings>
#include <nx/utils/argument_parser.h>


//!Able to take settings from \a QSettings class (win32 registry or ini file) or from command line arguments
/*!
    Value defined as command line argument has preference over registry
*/
class QnSettings
{
public:
    QnSettings(
        QSettings::Scope scope,
        const QString& organization,
        const QString& application = QString());
    QnSettings(
        const QString& fileName,
        QSettings::Format format);

    void parseArgs(int argc, char **argv);
    QVariant value(
        const QString& key,
        const QVariant& defaultValue = QVariant()) const;

private:
    QSettings m_systemSettings;
    nx::utils::ArgumentParser m_args;
};

#endif  //NX_SETTINGS_H
