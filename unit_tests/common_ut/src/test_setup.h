#include <QtCore/QString>

class TestSetup
{
public:
    static void setTemporaryDirectoryPath(const QString& path);
    static QString getTemporaryDirectoryPath();
};
