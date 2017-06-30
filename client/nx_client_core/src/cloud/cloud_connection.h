#pragma once

#include <memory>

#include <nx/cloud/cdb/api/connection.h>

#include <nx/utils/singleton.h>

class QnCloudConnectionProvider: public QObject, public Singleton<QnCloudConnectionProvider>
{
    Q_OBJECT

    using base_type = QObject;
public:
    QnCloudConnectionProvider(QObject* parent = nullptr);
    virtual ~QnCloudConnectionProvider();

    std::unique_ptr<nx::cdb::api::Connection> createConnection() const;

signals:
    void cdbEndpointChanged();

private:
    /* Factory must exist all the time we are using the connection. */
    std::unique_ptr<
        nx::cdb::api::ConnectionFactory,
        decltype(&destroyConnectionFactory)> connectionFactory;
};

#define qnCloudConnectionProvider QnCloudConnectionProvider::instance()
