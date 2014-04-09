#ifndef SERVER_UPDATE_UTIL_H
#define SERVER_UPDATE_UTIL_H

#include <QtCore/QObject>

class QnServerUpdateTool : public QObject
{
    Q_OBJECT
public:
    static QnServerUpdateTool *instance();

    bool addUpdateFile(const QString &updateId, const QByteArray &data);

    bool installUpdate(const QString &updateId);

protected:
    explicit QnServerUpdateTool();

private:
    static QnServerUpdateTool *m_instance;
};

#endif // SERVER_UPDATE_UTIL_H
