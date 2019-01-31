/*
 * Copyright 2017-present Network Optix, Inc.
 * This source code file is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */
import * as uuid from 'uuid/v4';

import config from '../../../config';
import { BaseEvent } from './baseEvent';

/**
 * Represents a soft trigger from the Event Rules.
 * @class
 */
export class SoftTrigger extends BaseEvent {
    /**
     * @param {string} buttonText The text that shows up on the soft trigger.
     * @param {string} icon The string value of the icon you want the soft trigger to show.
     * @constructor
     */
    constructor(buttonText?: string, icon?: string) {
        super(config.eventTypes.softwareTriggerEvent);
        this.config({buttonText, icon});
    }

    /**
     * Configures the soft trigger.
     * @param {string} buttonText The text that shows up on the soft trigger.
     * @param {string} icon The string value of the icon you want the soft trigger to show.
     */
    public config({buttonText= config.defaultSoftTriggerText, icon= config.softTriggerIcons.bell }: {
        buttonText?: string, icon?: string
    }) {
        this.eventCondition.caption = buttonText;
        this.eventCondition.description = icon;
        this.eventCondition.inputPortId = `{${uuid()}}`;
        this.eventCondition.metadata.instigators = [... config.eventConditionMetaInstigators];
    }
}
