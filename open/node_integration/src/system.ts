/*
 * Copyright 2017-present Network Optix, Inc.
 * This source code file is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */
declare const Promise: any;
import { EventRuleManager, MediaserverApi, Rule } from './index';
import { factory } from './logConfig';

const logging = factory.getLogger('System');

/**
 * Represents a system.
 * @class
 */
export class System extends MediaserverApi {
    public ruleManager: EventRuleManager; //< Helps manage rules on the system and in the code.
    public cameras: any = [];

    /**
     * @param {string} systemUrl
     * @param {string} username
     * @param {string} password
     * @param {{[p: string]: string}} configRules Rules from the config file.
     * @constructor
     */
    constructor(systemUrl: string, username: string, password: string,
                configRules: { [key: string]: string }) {
        super(systemUrl, username, password);
        this.ruleManager = new EventRuleManager({ configRules });
        this.cameras = this.getCameras();
        logging.info('Server is ready');
    }

    /**
     * Gets the rules from the system and sets them in the rule manager.
     * @returns {Bluebird<EventRuleManager>}
     */
    public getSystemRules() {
        return this.getRules().then((rules: any) => {
            return this.ruleManager.setRulesIds(rules);
        });
    }

    /**
     * Enables or disables a rule.
     * @param {Rule} rule
     * @param {boolean} state
     */
    public disableRule(rule: Rule, state: boolean) {
        rule.config({ disabled: state, id: rule.ruleId });
        this.saveRule(rule).then(() => {
            logging.info(`${rule.comment} is now ${state ? 'disabled' : 'enabled' }`);
        });
    }

    /**
     * Saves the rule to the system or sets the id of an already existing rule.
     * @param {Rule} rule
     * @returns {any}
     */
    public saveRuleToSystem(rule: Rule) {
        const ruleId = this.ruleManager.ruleExists(rule);
        if (ruleId !== '') {
            rule.ruleId = ruleId;
            return Promise.resolve(rule);
        } else {
            return this.saveRule(rule);
        }
    }
}
