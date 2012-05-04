#include "util.h"

QString getDataDirectory()
{
    return QDesktopServices::storageLocation(QDesktopServices::DataLocation);
}

