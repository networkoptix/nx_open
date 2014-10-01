#ifndef DISCOVERED_PEERS_REST_HANDLER_H
#define DISCOVERED_PEERS_REST_HANDLER_H

#include "rest/server/json_rest_handler.h"

class QnDiscoveredPeersRestHandler : public QnJsonRestHandler {
    Q_OBJECT
public:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result) override;
};

#endif // DISCOVERED_PEERS_REST_HANDLER_H
