#include "info.h"

#include <QtCore/QFile>
#include <QtCore/QTextStream>

#include "utils/network/nettools.h"

namespace
{
    QString readFile(const QString& file)
    {
        QString result;

        QFile f(file);
        if (!f.open(QFile::ReadOnly | QFile::Text))
            return result;

        QTextStream in(&f);
        result = in.readAll();
        f.close();

        return result.trimmed();
    }
}

QString Nx1::getMac()
{
    return getMacFromPrimaryIF().replace(QLatin1Char('-'), QLatin1Char(':'));
}

QString Nx1::getSerial()
{
    return readFile(lit("/tmp/serial"));
}
