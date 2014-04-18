#ifndef SERVER_UPDATE_UTIL_H
#define SERVER_UPDATE_UTIL_H

#include <QtCore/QObject>

#include <utils/common/system_information.h>
#include <utils/common/software_version.h>

class QnServerUpdateTool : public QObject {
    Q_OBJECT
public:
    struct UpdateInformation {
        QnSystemInformation systemInformation;
        QnSoftwareVersion version;
    };

    static QnServerUpdateTool *instance();

    bool addUpdateFile(const QString &updateId, const QByteArray &data);

    bool installUpdate(const QString &updateId);

protected:
    explicit QnServerUpdateTool();

private:
    static QnServerUpdateTool *m_instance;
};

#endif // SERVER_UPDATE_UTIL_H
