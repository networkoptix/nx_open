#pragma once

#include <QtCore/QObject>

// TODO: #GDM get rid of this class
class QnEnvironment
{
public:
    static void showInGraphicalShell(const QString &path);

    static QString getUniqueFileName(const QString &dirName, const QString &baseName);
};
