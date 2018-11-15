#ifndef IFCONFIG_REST_HANDLER_H
#define IFCONFIG_REST_HANDLER_H

#include "rest/server/json_rest_handler.h"
#include "network_address_entry.h"

class QnIfConfigRestHandler : public QnJsonRestHandler {
    Q_OBJECT
public:
    QnIfConfigRestHandler();

    virtual int executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
private:
    bool m_modified;

    bool checkData(const QnNetworkAddressEntryList& newSettings, QString* errString);
    virtual void afterExecute(const QString &path, const QnRequestParamList &params, const QByteArray& body, const QnRestConnectionProcessor* owner) override;
};

#endif // IFCONFIG_REST_HANDLER_H
