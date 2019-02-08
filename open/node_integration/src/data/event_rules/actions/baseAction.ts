/*
 * Copyright 2017-present Network Optix, Inc.
 * This source code file is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */
import config from '../../../config';

import { factory } from '../../../logConfig';

const logging = factory.getLogger('Base Action');

/**
 * Base class for all action classes.
 * @class
 */
export class BaseAction {
    /**
     * The type of action. See [[config.actionTypes]] for a list of supported actions.
     */
    public actionType: string;
    /**
     * Parameters related to the action part of a rule.
     */
    public actionParams: { [key: string]: any };

    /**
     * @param {string} actionType
     * @constructor
     */
    public constructor(actionType: string) {
        logging.info(`Action of type ${actionType} was created.`);
        this.actionType = actionType;
        this.actionParams = { ...config.actionParams };
    }
}
