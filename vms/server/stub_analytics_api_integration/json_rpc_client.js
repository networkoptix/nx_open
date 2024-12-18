// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

"use strict"

const { resolve } = require("path");

const WebSocket = require("ws").WebSocket;

const makeJsonRpcRequest = (method, data, id) => {
    let request = {
        jsonrpc: "2.0",
        method: method,
        params: data,
    };

    if (typeof id === "string" || typeof id === "number")
        request.id = id;

    return request;
}

const makeJsonRpcResponse = (result, requestId) => {
    return {
        jsonrpc: "2.0",
        id: requestId,
        result: result
    };
}

const makeJsonRpcErrorResponse = (error, requestId) => {
    return {
        jsonrpc: "2.0",
        id: requestId,
        error: error,
    };
}

const isJsonRpcResponse = (data) => {
    return data.jsonrpc === "2.0"
        && (data.method === null || typeof data.method === "undefined")
        && (data.id !== null && typeof data.id !== "undefined")
        && ((data.result !== null && typeof data.result !== "undefined")
            || (data.error !== null && typeof data.error !== "undefined"));
}

const isJsonRpcRequest = (data) => {
    return data.jsonrpc === "2.0"
        && (data.id !== null && typeof data.id !== "undefined")
        && (data.method !== null || typeof data.method !== "undefined")
};

const isJsonRpcNotification = (data) => {
    return data.jsonrpc === "2.0"
        && (data.id === null || typeof data.id === "undefined")
        && (data.method !== null && typeof data.method !== "undefined")
}

const resolveJsonRpcCallPromises = (jsonRpcResponse, pendingCallPromises) => {
    if (!pendingCallPromises.hasOwnProperty(jsonRpcResponse.id))
        return;

    if (jsonRpcResponse.result !== null && typeof jsonRpcResponse.result !== "undefined")
        pendingCallPromises[jsonRpcResponse.id].resolve(jsonRpcResponse);
    else
        pendingCallPromises[jsonRpcResponse.id].reject(jsonRpcResponse);

    delete pendingCallPromises[jsonRpcResponse.id];
}

class JsonRpcClient {
    webSocket = null;
    pendingCallPromises = {};
    requestHandlers = {};
    onOpenHandler = null;
    onCloseHandler = null;
    lastRequestId = 0;

    connect(url, options) {
        this.webSocket = new WebSocket(url, options);
        this.webSocket.on("open", () => {
            if (this.onOpenHandler)
                this.onOpenHandler();
        });

        this.webSocket.on("close", () => {
            console.log("JsonRpcClient: webSocket.on\"close\"");

            if (this.onCloseHandler)
                this.onCloseHandler();
        });

        this.webSocket.on("message", (data) => {
            console.log("JsonRpcClient: webSocket.on\"message\"", data);
            let message = null;
            try {
                message = JSON.parse(data);
                console.log("JsonRpcClient: Parsed as json:", message);
            } catch (exception) {
                console.log("JsonRpcClient: Got invalid message:". exception);
                return;
            }

            if (isJsonRpcResponse(message)) {
                resolveJsonRpcCallPromises(message, this.pendingCallPromises);
            }
            else if (isJsonRpcRequest(message)) {
                if (this.requestHandlers.hasOwnProperty(message.method)) {
                    this.requestHandlers[message.method](message)
                        .then((result) => {
                            if (typeof result == "undefined")
                                return;

                            let response = makeJsonRpcResponse(result, message.id);
                            console.log("RESULT", JSON.stringify(response, null, 4));
                            this.webSocket.send(JSON.stringify(response));
                        })
                        .catch((error) => {
                            console.log("ERROR", error, makeJsonRpcErrorResponse(error, message.id));
                            this.webSocket.send(
                                JSON.stringify(makeJsonRpcErrorResponse(error, message.id)));
                        });
                }
            }
            else if (isJsonRpcNotification(message)) {
                console.log("JsonRpcClient: Got notification.");
                if (this.requestHandlers.hasOwnProperty(message.method)) {
                    this.requestHandlers[message.method](message);
                }
            }
        });
    }

    disconnect() {
        if (this.webSocket)
        {
            this.onCloseHandler = null;
            this.webSocket.close();
        }

        this.webSocket = null;
    }

    call(method, data = null) {
        let request = makeJsonRpcRequest(method, data, ++this.lastRequestId);
        let promise = new Promise((resolve, reject) => {
            this.pendingCallPromises[this.lastRequestId] = {
                resolve: resolve,
                reject: reject
            };
        });

        this.webSocket.send(JSON.stringify(request));
        return promise;
    }

    notify(method, data) {
        let request = makeJsonRpcRequest(method, data);
        console.log("JsonRpcClient: Sending notification: ", JSON.stringify(request, null, 4));
        this.webSocket.send(JSON.stringify(request));
    }

    on(method, handler) {
        this.requestHandlers[method] = handler;
    }

    off(method) {
        delete this.requestHandlers[method];
    }

    onOpen(handler) {
        this.onOpenHandler = handler;
    }

    onClose(handler) {
        this.onCloseHandler = handler;
    }
}

module.exports = {
    JsonRpcClient: JsonRpcClient
};
