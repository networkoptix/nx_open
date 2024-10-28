// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

/**@file
 * Intended to be included into the Server code.
 */

void LocalConnectionFactory::registerTransactionBusHandlers(
    QnHttpConnectionListener* httpConnectionListener,
    const UnauthorizedWatcher& unauthorizedWatcher)
{
    /**%apidoc GET /ec2/transactionBus/websocket
     * %deprecated Use <b>GET /jsonrpc</b> subscriptions for REST endpoints instead.
     * <p>
     * <b>ATTENTION:</b> The output data structure depends on the proprietary database structure
     * which may change in any VMS Server version without further notice. Use at your own risk.
     * </p>
     * Allows third-party systems to get access to the Server Transaction Bus. A third-party
     * peer should undergo the usual (the same as when accessing Server API or Site API)
     * authentication process before using a WebSocket connection. Transactions are transferred
     * in the JSON format by default. Example: ws://10.0.2.1/ec2/transactionBus/websocket
     * <br/>
     * Refer to the "Transaction Bus" section in the API function tree for the documentation.
     *
     * %caption Transaction Bus (WebSocket)
     * %ingroup Utilities
     * %return A ready-to-use WebSocket connection, read-only as of now.
     */
    httpConnectionListener->addHandler<nx::p2p::ConnectionProcessor>("HTTP",
        httpConnectionListener->normalizedPath(nx::p2p::ConnectionBase::kWebsocketUrlPath),
        unauthorizedWatcher);

    /**%apidoc GET /ec2/transactionBus/http
     * %deprecated Use <b>GET /jsonrpc</b> subscriptions for REST endpoints instead.
     * <p>
     * <b>ATTENTION:</b> The output data structure depends on the proprietary database structure
     * which may change in any VMS Server version without further notice. Use at your own risk.
     * </p>
     * Allows third-party systems to get access to the Server Transaction Bus.
     * Transactions are transferred in the JSON format by default.
     * Example: http://10.0.2.1/ec2/transactionBus/http
     * <br/>
     * Refer to the "Transaction Bus" section in the API function tree for the documentation.
     *
     * %caption Transaction Bus (HTTP)
     * %ingroup Utilities
     * %param[opt]:option noInitialData If specified, then the initial database is not sent to a
     *     Client on the connection start.
     * %return A ready-to-use HTTP connection, read-only as of now.
     */
    httpConnectionListener->addHandler<nx::p2p::ConnectionProcessor>("HTTP",
        httpConnectionListener->normalizedPath(nx::p2p::ConnectionBase::kHttpBaseUrlPath),
        unauthorizedWatcher);
}
