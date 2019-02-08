/*
 * Copyright 2017-present Network Optix, Inc.
 * This source code file is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */
declare const Promise: any;
import {
    HttpAction, Logger, NodeHttpAction, NodeServer,
    Rule, SoftTrigger, System
} from '../src';

import * as express from 'express';
import * as fs from 'fs';
import * as path from 'path';

const logging = Logger.getLogger('On Soft Trigger Click Do Http Action');

// Saves the rule's comment/name and id to the config file.
const saveRuleToConfig = (rule: Rule | null) => {
    if (rule === null) {
        return;
    }
    config.rules[rule.comment] = rule.ruleId;
    fs.writeFileSync(path.resolve(__dirname, './nodeConfig.json'),
            JSON.stringify(config, undefined, 4));
};

/*
 * After all of the rules have been generated this function executes.
 * The last rule created will be disabled and logged to the terminal.
 */
const rulesGenerationComplete = (rules: Rule[]) => {
    const lastIndex: number = rules.length - 1;
    logging.info('All rules made on system.');
    server.disableRule(rules[lastIndex], true);
    server.getRules(rules[lastIndex].ruleId).then((rule) => {
        logging.info(JSON.stringify(rule));
    });
};

// Makes all of the rules on the target system.
const makeExampleRules = () => {
    const promiseRules: any[] = [];
    /*
     * Makes a rule that sends an http action to the /welcome route on the express server
     * when the soft trigger is pressed.
     */
    const softTrigger: SoftTrigger = new SoftTrigger('redirect');
    const httpAction: HttpAction = new HttpAction(`${config.myIp}:${config.myPort}/welcome/`);
    const rule2 = new Rule('Soft Trigger Http Action').on(softTrigger).do(httpAction);
    promiseRules.push(server.saveRuleToSystem(rule2).then((rule: Rule | null) => {
        saveRuleToConfig(rule);
        return rule;
    }));

    /*
     * Makes a rule that sends an http action to the default route on the express server
     * when the soft trigger is pressed. In this example we are using the _config function of
     * the NodeHttpAction class to set the route and callback functions. However, they can be passed
     * into the constructor list this
     * new NodeHttpAction(nodeServer, 'test', () => {logging.info('Callback works');.
     * If you choose to set the route and callback function when the object is created you need to
     * pass in both of them.
     */
    const nodeHttpAction: NodeHttpAction = new NodeHttpAction(nodeServer);
    const softTrigger2: SoftTrigger = new SoftTrigger('Node callback');
    nodeHttpAction.configDefaultHandler('test', () => {
        logging.info('Callback works');
    });
    const rule4 = new Rule('Soft Trigger Http Action -> node')
            .on(softTrigger2)
            .do(nodeHttpAction);
    promiseRules.push(server.saveRuleToSystem(rule4).then((rule: Rule | null) => {
        saveRuleToConfig(rule);
        return rule;
    }));

    return Promise.all(promiseRules).then((values: any) => values);
};

// Gets the rules from the target system and add a custom route to the express server.
const addCustomExpressRoute = () => {
    nodeServer.addExpressHandler('/welcome/', (req: express.Request, res: express.Response) => {
        logging.info('New welcome route!');
        res.redirect('https://www.google.com/');
    });
};

const config = require('./nodeConfig');
const server: System = new System(config.systemUrl, config.username, config.password, config.rules);
const nodeServer: NodeServer = new NodeServer(config.myIp, config.myPort);
addCustomExpressRoute();
server.getSystemRules().then(makeExampleRules).then(rulesGenerationComplete);
