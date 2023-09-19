// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "file_io.h"

#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtQml/QtQml>

#include <nx/utils/log/log.h>

namespace nx::vms::client::core {

void FileIO::registerQmlType()
{
    qmlRegisterSingletonType<FileIO>("nx.vms.client.core", 1, 0, "FileIO",
        [](QQmlEngine* /*engine*/, QJSEngine* /*jsEngine*/)
        {
            return new FileIO();
        });
}

QString FileIO::readAll(const QString& fileName) const
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
    {
        NX_DEBUG(this, QString("Can't open file <%1>, error: ")
            .arg(fileName)
            .arg(file.errorString()));
        return {};
    }

    QTextStream stream(&file);
    return stream.readAll();
}

} // namespace nx::vms::client::core
