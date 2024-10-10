// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

"use strict"

const clearCanvas = (context) => {
    context.clearRect(0, 0, context.canvas.width, context.canvas.height);
};

const translateToCanvasCoordinateSystem= (context, rect) => {
    const width = context.canvas.width;
    const height = context.canvas.heigh;
    const scaled = {};

    console.log("Size:", width, height);

    scaled.x = rect.x * width;
    scaled.y = rect.y * height;
    scaled.width = rect.width * width;
    scaled.height = rect.height * height;

    console.log("Scaled rect:", scaled);
    return scaled;
};

const translateToLogicalCoordinateSystem = (context, rect) => {
    const width = context.canvas.width;
    const height = context.canvas.height;

    const scaled = {};
    scaled.x = rect.x / width;
    scaled.y = rect.y / height;
    scaled.width = rect.width / width;
    scaled.height = rect.height / height;

    console.log("Scaled:", scaled);

    return scaled;
};

const drawObject = (context, rect) => {
    context.strokeRect(rect.x, rect.y, rect.width, rect.height);
};

const isPointInRect = (point, rect) => {
    return point.x > rect.x && point.x < rect.x + rect.width
        && point.y > rect.y && point.y < rect.y + rect.height;
};

const appContext = {
    object: {
        x: 100,
        y: 100,
        width: 100,
        height: 100
    },
    movementContext: null,
    canvasContext: null
};

const onMousedown = (event) => {
    const point = {x: event.offsetX, y: event.offsetY};
    if (!isPointInRect(point, appContext.object))
        return;

    appContext.movementContext = {
        capturePointOffsetX: event.offsetX - appContext.object.x,
        capturePointOffsetY: event.offsetY - appContext.object.y
    };
};

const onMouseup = (event) => {
    appContext.movementContext = null;
};

const onMousemove = (event) => {
    if (!appContext.movementContext)
        return;

    clearCanvas(appContext.canvasContext);
    appContext.object.x = event.offsetX - appContext.movementContext.capturePointOffsetX;
    appContext.object.y = event.offsetY - appContext.movementContext.capturePointOffsetY;
    drawObject(appContext.canvasContext, appContext.object);
    window.mainProcess.ipc.send(
        "object-move",
        translateToLogicalCoordinateSystem(appContext.canvasContext, appContext.object))
};

const renderState = (state) => {
    const connectionButton = document.getElementById("connection-button");
    const connectionStatusDisplay = document.getElementById("connection-status");

    connectionButton.textContent = state.connectionState == "connected"
        ? "Disconnect RPC client(as an Integration)"
        : "Connect RPC client(as an Integration)";


    connectionStatusDisplay.textContent = state.connectionState == "connected"
        ? "Connected"
        : "Disconnected";
}

const initCanvas = () => {
    const canvas = document.getElementById("video-canvas");
    canvas.width = canvas.clientWidth;
    canvas.height = canvas.clientHeight;
    canvas.addEventListener("mousedown", onMousedown);
    canvas.addEventListener("mousemove", onMousemove);
    canvas.addEventListener("mouseup", onMouseup);

    appContext.canvasContext = canvas.getContext("2d");
    drawObject(appContext.canvasContext, appContext.object);
    window.mainProcess.ipc.send(
        "object-move",
        translateToLogicalCoordinateSystem(appContext.canvasContext, appContext.object)
    );
}

const initControls = () => {
    const connectButton = document.getElementById("connection-button");
    connectButton.addEventListener("click", () => {
        window.mainProcess.ipc.send("connect-disconnect");
    });

    const makeIntegrationRequestButton = document.getElementById("make-integration-request-button");
    makeIntegrationRequestButton.addEventListener("click", () => {
        window.mainProcess.ipc.send("make-integration-request");
    });

    const approveIntegrationRequestButton =
        document.getElementById("approve-integration-request-button");
    approveIntegrationRequestButton.addEventListener("click", () => {
        window.mainProcess.ipc.send("approve-integration-request");
    });

    const removeIntegrationButton = document.getElementById("remove-integration-button");
    removeIntegrationButton.addEventListener("click", () => {
        window.mainProcess.ipc.send("remove-integration");
    });
}

const initNotifications = () => {
    window.mainProcess.ipc.on("state-changed", (event, data) => {
        console.log("State changed, re-rendering state");
        renderState(data);
    });
}

const onReady = () => {
    initNotifications();
    initCanvas();
    initControls();
};

document.addEventListener("DOMContentLoaded", onReady);
