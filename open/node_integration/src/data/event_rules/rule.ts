/*
 * Copyright 2017-present Network Optix, Inc.
 * This source code file is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */
import { BaseAction, BaseEvent, GenericEvent, ShowPopup } from './index';

import { factory } from '../../logConfig';

const logging = factory.getLogger('Rule');

/**
 * Represents an event rule on the system.
 * @class
 */
export class Rule {
    /**
     * Comment is the comment that shows up in the event rules on the system. Also it is used
     * as a name for the rule on the express server from the @type{NodeSerer} class.
     */
    public comment: string;
    /**
     * The id of the rule on the system
     */
    public ruleId?: string;
    /**
     * Action holds the action parameters of a rule.
     * By default it is a ShowPopup action.
     * @type {ShowPopup}
     */
    public action: BaseAction = new ShowPopup();
    /**
     * Event holds the event parameters of a rule.
     * By default it is a Generic event.
     * @type {GenericEvent}
     */
    public event: BaseEvent = new GenericEvent();
    /**
     * cameraIds is a list of camera ids where the rule will be made.
     * @type {any[]}
     */
    private cameraIds: string[] = [];
    /**
     * Used to enable or disable the rule.
     * @type {boolean}
     */
    private disabled = false;

    /**
     * @param {string} comment Comment on the server and name for the rule.
     * @param {boolean} disabled States whether or not the rule is disabled
     * @constructor
     */
    constructor(comment: string, disabled?: boolean) {
        this.comment = comment;
        if (disabled !== undefined) {
            this.disabled = disabled;
        }
    }

    /**
     * Sets a rule's id, whether its enabled or disabled, and/or the cameraIds associated with the
     * rule.
     * @param {[p: string]: any} params
     */
    public config(params: { [key: string]: any }) {
        if (params.disabled !== undefined) {
            this.disabled = params.disabled;
        }
        if (params.cameraIds !== undefined) {
            this.cameraIds = params.cameraIds;
        }
        if (params.id !== undefined) {
            this.ruleId = params.id;
        }
    }

    /**
     * Sets the action for the rule.
     * @param {BaseAction} action Takes any of the inherited actions.
     * @returns {this} Allows for chaining.
     */
    public do(action: BaseAction) {
        this.action = action;
        return this;
    }

    /**
     * Sets the event for the rule.
     * @param {BaseEvent} event Takes any of the inherited events.
     * @returns {this} Allows for chaining.
     */
    public on(event: BaseEvent) {
        this.event = event;
        return this;
    }

    /**
     * Converts the object into a json.
     * @returns {{[p: string]: any}}
     */
    public makeRuleJson() {
        if (typeof (this.action) === 'undefined' || typeof (this.event) === 'undefined') {
            const errorMsg = 'Action and Event must both be defined to make rules';
            throw new TypeError(errorMsg);
        }

        const ruleJson: any = {
            actionParams: JSON.stringify(this.action.actionParams),
            actionResourceIds: [],
            actionType: (this.action.actionType),
            aggregationPeriod: 0,
            comment: `${this.comment}`,
            disabled: (this.disabled),
            eventCondition: JSON.stringify(this.event.eventCondition),
            eventResourceIds: this.cameraIds,
            eventState: 'Undefined',
            eventType: (this.event.eventType),
            schedule: '',
            system: false
        };

        if (this.ruleId !== undefined) {
            ruleJson.id = this.ruleId;
        }
        return ruleJson;
    }
}
