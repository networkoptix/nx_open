// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

"use strict"

const { Engine } = require("./engine");

class Integration {
    manifests = {};
    mouseMovableObjectMetadata = null;
    rpcClient = null;

    constructor(manifests, mouseMovableObjectMetadata, rpcClient)
    {
        this.manifests = manifests;
        this.mouseMovableObjectMetadata = mouseMovableObjectMetadata;
        this.rpcClient = rpcClient
    }

    manifest()
    {
        return this.manifests.integrationManifest;
    }

    obtainEngine(engineId)
    {
        return new Engine(engineId,
            this.mouseMovableObjectMetadata,
            this.rpcClient,
            this.manifests);
    }
}

module.exports = {
    Integration: Integration
};
