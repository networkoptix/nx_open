#include "server_update_tool.h"

#include <QtCore/QDir>

namespace {

    const QString updatesSuffix = lit("mediaserver/updates");
    const QString updatePrefix = lit("update_");

    QDir getUpdatesDir() {
        QDir dir = QDir::temp();
        if (!dir.exists(updatesSuffix))
            dir.mkpath(updatesSuffix);
        dir.cd(updatesSuffix);
        return dir;
    }

    QString getUpdateFilePath(const QString &updateId) {
        return getUpdatesDir().absoluteFilePath(updatePrefix + updateId);
    }

} // anonymous namespace

QnServerUpdateTool *QnServerUpdateTool::m_instance = NULL;

QnServerUpdateTool::QnServerUpdateTool() {
}

QnServerUpdateTool *QnServerUpdateTool::instance() {
    if (!m_instance)
        m_instance = new QnServerUpdateTool();

    return m_instance;
}

bool QnServerUpdateTool::addUpdateFile(const QString &updateId, const QByteArray &data) {
    QFile file(getUpdateFilePath(updateId));
    if (!file.open(QFile::WriteOnly))
        return false;

    return data.size() == file.write(data); // file will be closed automatically in ~QFile()
}

bool QnServerUpdateTool::installUpdate(const QString &updateId) {
    QString updateFile = getUpdateFilePath(updateId);
    if (!QFile::exists(updateFile))
        return false;

    return true;
}
