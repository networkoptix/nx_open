// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

"use strict";

const { contextBridge, ipcRenderer } = require('electron');

// TODO: it's unsafe, fix it.
contextBridge.exposeInMainWorld('mainProcess', {
    ipc: {
        send: (eventName, data) => {
            ipcRenderer.send(eventName, data);
        },

        on: (eventName, handler) => {
            ipcRenderer.on(eventName, handler);
        }
    }
});
