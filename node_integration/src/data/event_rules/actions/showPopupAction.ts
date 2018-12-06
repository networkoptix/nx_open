/*
 * Copyright 2017-present Network Optix, Inc.
 * This source code file is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */
import config from '../../../config';
import { BaseAction } from './baseAction';

/**
 * ShowPopup represents the show notification action in the Event Rules.
 * @class
 */
export class ShowPopup extends BaseAction {
    /**
     * @constructor
     */
    constructor() {
        super(config.actionTypes.showPopupAction);
        this.actionParams.additionalResources = [...config.actionParamsAdditionalResources];
    }

    /**
     * This function is not currently used.
     * @ignore
     */
    public config() {
        return;
    }
}
