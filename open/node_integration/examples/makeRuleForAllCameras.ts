/*
 * Copyright 2017-present Network Optix, Inc.
 * This source code file is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */
import { Logger, NodeHttpAction, NodeServer, Rule, SoftTrigger, System } from '../src';

import * as express from 'express';
import * as fs from 'fs';
import * as path from 'path';

const logging = Logger.getLogger('Make Rule For All Cameras in the System');
// Saves the rule's comment/name and id to the config file.
const saveRuleToConfig = (rule: Rule | null) => {
    if (rule === null) {
        return;
    }
    config.rules[rule.comment] = rule.ruleId;
    fs.writeFileSync(path.resolve(__dirname, './nodeConfig.json'),
            JSON.stringify(config, undefined, 4));
};
// Makes a rule for each camera currently on the system.
const makeExampleRules = (cameras: any) => {
    cameras = cameras.map((camera: any) => ({ id: camera.id, name: camera.name }));
    const customRoute = 'shark?state=open&cameraName=';

    for (const camera of cameras) {
        const softTrigger: SoftTrigger = new SoftTrigger(`Open - ${camera.name}`);
        const httpAction: NodeHttpAction = new NodeHttpAction(nodeServer);
        httpAction.configCustomHandler(`${customRoute}${camera.name}`,
                (query: any) => {
            logging.info(`${query.cameraName} - was just ${query.state}`);
        });
        // Makes a rule that sends an http action to the express server when a soft trigger is pushed.
        const cameraRule = new Rule(`Open - ${camera.name}`)
                .on(softTrigger)
                .do(httpAction);
        cameraRule.config({ cameraIds: [camera.id] });
        server.saveRuleToSystem(cameraRule).then(saveRuleToConfig);
    }
};

// Adds a new route to the express server, and gets all of the cameras in the system.
const getCameras = () => {
    nodeServer.addExpressHandler('/shark',
            (req: express.Request, res: express.Response) => {
        logging.info(JSON.stringify(req.query));
        const key = `${req.query.state} - ${req.query.cameraName}`;
        if (key in nodeServer.httpActionCallbacks) {
            nodeServer.httpActionCallbacks[key](req.query);
            return res.send('Ok');
        }
        return res.send('Error: Query parameters do not have a callback function.');
    });
    return server.getCameras();
};

const config = require('./nodeConfig');
const server: System = new System(config.systemUrl, config.username, config.password, config.rules);
const nodeServer: NodeServer = new NodeServer(config.myIp, config.myPort);
server.getSystemRules().then(getCameras).then(makeExampleRules);
