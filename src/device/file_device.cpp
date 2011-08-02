#include "file_device.h"

#include "../streamreader/single_shot_file_reader.h"

CLFileDevice::CLFileDevice(const QString &filename)
{
    QFileInfo fi(filename);
    setName(fi.fileName());
    setUniqueId(fi.absoluteFilePath());
    addDeviceTypeFlag(CLDevice::SINGLE_SHOT);
}

CLStreamreader* CLFileDevice::getDeviceStreamConnection()
{
    return new CLSingleShotFileStreamreader(this);
}
