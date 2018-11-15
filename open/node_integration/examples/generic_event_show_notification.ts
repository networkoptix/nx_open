/*
 * Copyright 2017-present Network Optix, Inc.
 * This source code file is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */
declare const Promise: any;
import {
    GenericEvent, Logger, Rule, ShowPopup, System
} from '../src';

import * as fs from 'fs';
import * as path from 'path';

const logging = Logger.getLogger('On Generic Event Show Notification');

// Saves the rule's comment/name and id to the config file.
const saveRuleToConfig = (rule: Rule | null) => {
    if (rule === null) {
        return;
    }
    config.rules[rule.comment] = rule.ruleId;
    fs.writeFileSync(path.resolve(__dirname, './nodeConfig.json'),
            JSON.stringify(config, undefined, 4));
};

// Makes all of the rules on the target system.
const makeExampleRules = () => {
    const promiseRules: any[] = [];
    /*
     * Makes a rule that displays a notification when a generic event with 'node.js' as the source
     * is received.
     */
    const genericEvent: GenericEvent = new GenericEvent('Node.js');
    const showNotification: ShowPopup = new ShowPopup();
    const catchSpecificEvent = new Rule('Generic Event Popup', false)
            .on(genericEvent)
            .do(showNotification);
    // After this rule is made, test it by triggering the event manually.
    promiseRules.push(server.saveRuleToSystem(catchSpecificEvent)
            .then((rule: Rule | null) => {
                server.sendEvent({ event: genericEvent });
                saveRuleToConfig(rule);
                return rule;
            })
    );

    // Makes a rule that displays a notification whenever a generic event is received.
    const genericEvent2: GenericEvent = new GenericEvent();
    genericEvent2.config({});
    const catchAllEvents: Rule = new Rule('Catch All Events')
            .on(genericEvent2)
            .do(showNotification);
    promiseRules.push(server.saveRuleToSystem(catchAllEvents).then((rule: Rule | null) => {
        saveRuleToConfig(rule);
        return rule;
    }));

    return Promise.all(promiseRules).then((values: any) => values);
};

const config = require('./nodeConfig');
const server: System = new System(config.systemUrl, config.username, config.password, config.rules);
server.getSystemRules().then(makeExampleRules);
