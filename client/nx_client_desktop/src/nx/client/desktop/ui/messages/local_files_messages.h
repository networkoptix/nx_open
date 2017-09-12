#pragma once

#include <QtCore/QCoreApplication>

#include <core/resource/resource_fwd.h>
#include <core/resource_access/resource_access_subject.h>
#include <client/client_globals.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace messages {

class LocalFiles
{
    Q_DECLARE_TR_FUNCTIONS(LocalFiles)
public:
    static void pathInvalid(QWidget* parent, const QString& path);
    static void fileExists(QWidget* parent, const QString& filename);
    static void fileIsBusy(QWidget* parent, const QString& filename);
    static void fileCannotBeWritten(QWidget* parent, const QString& filename);
    static void invalidChars(QWidget* parent, const QString& invalidSet);
    static void reservedFilename(QWidget* parent, const QString& filename);
};

} // namespace messages
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
