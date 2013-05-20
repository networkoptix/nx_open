
#include "util.h"

#include <QtGui/QDesktopServices>


QString getDataDirectory()
{
    return QDesktopServices::storageLocation(QDesktopServices::DataLocation);
}

