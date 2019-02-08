/*
 * Copyright 2017-present Network Optix, Inc.
 * This source code file is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */
import config from '../../../config';
import { factory } from '../../../logConfig';

const logging = factory.getLogger('Base Event');

/**
 * Base event class for all other event classes.
 * @class
 */
export class BaseEvent {
    /**
     * The type of event. See [[config.eventTypes]] for a list of supported events.
     */
    public eventType: string;
    /**
     * Parameters related to the event part of a rule.
     */
    public eventCondition: { [key: string]: any };

    /**
     * @param {string} eventType
     * @constructor
     */
    public constructor(eventType: string) {
        logging.info(`Event of type ${eventType} was created.`);
        this.eventType = eventType;
        this.eventCondition = { ...config.eventCondition };
    }

    /**
     * Turns the class object into a querystring.
     * @returns {{}}
     * @ignore
     */
    public formatQueryString() {
        return {};
    }
}
