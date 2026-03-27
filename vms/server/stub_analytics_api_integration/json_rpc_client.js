// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

"use strict"

const WebSocket = require("ws").WebSocket;
const { Buffer } = require("buffer");
const { createLogger } = require("./utils.js");

const kMaxPayloadLogLength = 4000;
const logger = createLogger("RPC");

const safeStringify = (value) => {
    try
    {
        return JSON.stringify(value);
    }
    catch (error)
    {
        return "<non-serializable>";
    }
};

const toLogString = (value) => {
    if (Buffer.isBuffer(value))
        return value.toString("utf8");

    if (typeof value === "string")
        return value;

    if (typeof value === "undefined" || value === null)
        return "";

    return safeStringify(value);
};

const truncateForLog = (text, maxLength = kMaxPayloadLogLength) => {
    if (typeof text !== "string")
        text = String(text);

    if (text.length <= maxLength)
        return text;

    return text.slice(0, maxLength) + "...<truncated>";
};

const pendingCallsCount = (pendingCallPromises) => Object.keys(pendingCallPromises).length;

const messageSummary = (message) => {
    const hasResult = message && Object.prototype.hasOwnProperty.call(message, "result");
    const hasError = message && Object.prototype.hasOwnProperty.call(message, "error");
    return "id=" + message.id
        + ", method=" + message.method
        + ", hasResult=" + hasResult
        + ", hasError=" + hasError;
};

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
        return false;

    if (jsonRpcResponse.result !== null && typeof jsonRpcResponse.result !== "undefined")
        pendingCallPromises[jsonRpcResponse.id].resolve(jsonRpcResponse);
    else
        pendingCallPromises[jsonRpcResponse.id].reject(jsonRpcResponse);

    delete pendingCallPromises[jsonRpcResponse.id];
    return true;
}

const safeSend = (websocket, data) => {
    if (!websocket)
    {
        logger.warn("WebSocket is null. Unable to send data.");
        return false;
    }

    if (websocket.readyState === WebSocket.OPEN)
    {
        websocket.send(data);
        return true;
    }
    else
    {
        logger.warn("WebSocket is not open. Unable to send data. readyState=", websocket.readyState);
        return false;
    }
}

class JsonRpcClient {
    webSocket = null;
    pendingCallPromises = {};
    requestHandlers = {};
    onOpenHandler = null;
    onCloseHandler = null;
    lastRequestId = 0;

    connect(url, options) {
        logger.log("connect() called. url=", url);
        if (this.webSocket)
        {
            logger.warn("Existing websocket detected. It will be replaced. readyState=", this.webSocket.readyState);
        }

        this.webSocket = new WebSocket(url, options);

        this.webSocket.on("error", (error) => {
            logger.error("webSocket.on\"error\"", error);
        });

        this.webSocket.on("open", () => {
            logger.log("webSocket.on\"open\"",
                "pendingCalls=", pendingCallsCount(this.pendingCallPromises),
                "handlers=", Object.keys(this.requestHandlers).length);

            if (this.onOpenHandler)
                this.onOpenHandler();
        });

        this.webSocket.on("close", (code, reasonBuffer) => {
            const reason = reasonBuffer ? reasonBuffer.toString("utf8") : "";
            logger.log("webSocket.on\"close\"",
                "code=", code,
                "reason=", reason,
                "pendingCalls=", pendingCallsCount(this.pendingCallPromises));

            if (this.onCloseHandler)
                this.onCloseHandler();
        });

        this.webSocket.on("message", (data, isBinary) => {
            const dataAsString = toLogString(data);
            logger.log("webSocket.on\"message\"",
                "isBinary=", isBinary,
                "size=", dataAsString.length,
                "payload=", truncateForLog(dataAsString));

            let message = null;
            try
            {
                message = JSON.parse(dataAsString);
                logger.log("Parsed JSON message:", messageSummary(message));
            }
            catch (exception)
            {
                logger.warn("Got invalid JSON message. error=", exception);
                return;
            }

            if (isJsonRpcResponse(message))
            {
                const pendingBefore = pendingCallsCount(this.pendingCallPromises);
                const resolved = resolveJsonRpcCallPromises(message, this.pendingCallPromises);
                const pendingAfter = pendingCallsCount(this.pendingCallPromises);

                if (!resolved)
                {
                    logger.warn("Response with unknown id received.",
                        "id=", message.id,
                        "pendingBefore=", pendingBefore,
                        "pendingAfter=", pendingAfter);
                    return;
                }

                logger.log("Resolved response.",
                    messageSummary(message),
                    "pendingBefore=", pendingBefore,
                    "pendingAfter=", pendingAfter);
            }
            else if (isJsonRpcRequest(message))
            {
                if (!this.requestHandlers.hasOwnProperty(message.method))
                {
                    logger.warn("No handler registered for request method.",
                        "method=", message.method,
                        "id=", message.id);
                    return;
                }

                logger.log("Handling request.", "method=", message.method, "id=", message.id);

                Promise.resolve()
                    .then(() => this.requestHandlers[message.method](message))
                    .then((result) => {
                        if (typeof result == "undefined")
                        {
                            logger.warn("Request handler returned undefined. No response will be sent.",
                                "method=", message.method,
                                "id=", message.id);
                            return;
                        }

                        let response = makeJsonRpcResponse(result, message.id);
                        logger.log("Sending request response.",
                            "method=", message.method,
                            "id=", message.id,
                            "payload=", truncateForLog(safeStringify(response)));
                        safeSend(this.webSocket, JSON.stringify(response));
                    })
                    .catch((error) => {
                        const errorResponse = makeJsonRpcErrorResponse(error, message.id);
                        logger.error("Request handler failed.",
                            "method=", message.method,
                            "id=", message.id,
                            "error=", error);
                        safeSend(this.webSocket, JSON.stringify(errorResponse));
                    });
            }
            else if (isJsonRpcNotification(message))
            {
                logger.log("Got notification.", "method=", message.method);

                if (!this.requestHandlers.hasOwnProperty(message.method))
                {
                    logger.warn("No handler registered for notification method.", "method=", message.method);
                    return;
                }

                Promise.resolve()
                    .then(() => this.requestHandlers[message.method](message))
                    .then(() => {
                        logger.log("Notification handled successfully.", "method=", message.method);
                    })
                    .catch((error) => {
                        logger.error("Notification handler failed.", "method=", message.method, "error=", error);
                    });
            }
            else
            {
                logger.warn("Received message that is neither request, response nor notification.",
                    "payload=", truncateForLog(safeStringify(message)));
            }
        });
    }

    disconnect() {
        logger.log("disconnect() called.",
            "hasSocket=", !!this.webSocket,
            "pendingCalls=", pendingCallsCount(this.pendingCallPromises));

        if (this.webSocket)
        {
            this.onCloseHandler = null;
            this.webSocket.close();
        }

        this.webSocket = null;
    }

    call(method, data = null) {
        const requestId = ++this.lastRequestId;
        let request = makeJsonRpcRequest(method, data, requestId);
        const pendingBefore = pendingCallsCount(this.pendingCallPromises);

        logger.log("call()",
            "method=", method,
            "id=", requestId,
            "pendingBefore=", pendingBefore,
            "payload=", truncateForLog(safeStringify(request)));

        let promise = new Promise((resolve, reject) => {
            this.pendingCallPromises[requestId] = {
                resolve: resolve,
                reject: reject
            };
        });

        const sent = safeSend(this.webSocket, JSON.stringify(request));
        logger.log("call() send result",
            "method=", method,
            "id=", requestId,
            "sent=", sent,
            "pendingAfter=", pendingCallsCount(this.pendingCallPromises));

        return promise;
    }

    notify(method, data) {
        let request = makeJsonRpcRequest(method, data);
        logger.log("notify()",
            "method=", method,
            "payload=", truncateForLog(safeStringify(request)));

        const sent = safeSend(this.webSocket, JSON.stringify(request));
        logger.log("notify() send result", "method=", method, "sent=", sent);
    }

    on(method, handler) {
        logger.log("Registering handler.", "method=", method);
        this.requestHandlers[method] = handler;
    }

    off(method) {
        logger.log("Removing handler.", "method=", method);
        delete this.requestHandlers[method];
    }

    onOpen(handler) {
        logger.log("Registering onOpen handler.");
        this.onOpenHandler = handler;
    }

    onClose(handler) {
        logger.log("Registering onClose handler.");
        this.onCloseHandler = handler;
    }
}

module.exports = {
    JsonRpcClient: JsonRpcClient
};
