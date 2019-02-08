/*
 * Copyright 2017-present Network Optix, Inc.
 * This source code file is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */
import * as request from 'request-promise';
import { BaseEvent, Rule } from './index';
import { factory } from './logConfig';

const logging = factory.getLogger('Mediaserver Api');

/**
 * A class that uses some basic api calls to interact with a mediaserver.
 * @class
 */
export class MediaserverApi {
    /** The IP address and port of the target mediaserver. */
    public systemUrl: string;
    protected user: string;
    protected password: string;

    /**
     * @param {string} systemUrl Ip address and port of the target system.
     * @param {string} username Valid username on the system.
     * @param {string} password Password belonging to that user.
     * @constructor
     */
    constructor(systemUrl: string, username: string, password: string) {
        logging.info('Building api system');
        this.user = username;
        this.password = password;
        this.systemUrl = systemUrl;
    }

    /**
     * Logs relevant information about parameters for this class.
     */
    public display() {
        logging.info(() => JSON.stringify({
            systemUrl: this.systemUrl,
            user: this.user
        }));
    }

    /**
     * Pings the system to check if its active and gets general information about it.
     * @returns {Bluebird<void>}
     */
    public ping() {
        return this.getWrapper('/api/moduleInformation')
                .then((res: any) => {
                    logging.info(res.reply);
                });
    }

    /**
     * Gets the system's settings.
     * @returns {Bluebird<void>}
     */
    public getSystemSettings() {
        return this.getWrapper('/ec2/getSettings')
                .then((res: any) => {
                    logging.info(res);
                }, (err: any) => {
                    logging.error(err);
                });
    }

    /**
     * Gets all of the cameras from system.
     * @returns {Bluebird<void>}
     */
    public getCameras() {
        return this.getWrapper('/ec2/getCamerasEx', {})
                .then((cameras: any) => {
                    return cameras;
                }, (err: any) => {
                    logging.error(err);
                    return [];
                });
    }

    /**
     * Gets the rules currently on the system and saves the rule ids.
     * If a ruleId is passed into this method it returns that rule.
     * @param {string} ruleId Id of a rule
     * @returns {Bluebird<any>}
     */
    public getRules(ruleId?: string) {
        const qs: object = ruleId !== undefined ? { id: ruleId } : {};
        return this.getWrapper('/ec2/getEventRules', qs)
                .then((rules: any) => {
                    return ruleId === undefined ? rules : rules[0];
                });
    }

    /**
     * Saves a rule to the system.
     * @param {Rule} rule The rule you are trying to save to a system.
     * @returns {any}
     */
    public saveRule(rule: Rule) {
        return this.postWrapper('/ec2/saveEventRule', rule.makeRuleJson())
                .then((res: any) => {
                    rule.ruleId = res.id;
                    return rule;
                }, (err: any) => {
                    logging.error(err);
                    return undefined;
                });
    }

    /**
     * Sends an event to system. Currently only supports generic events.
     * @param {BaseEvent} event The event you want to trigger on the server.
     * @param {Date} timestamp The timestamp of when the event occurred.
     * @param {string} state Describes if the event started or stopped.
     *     Values can be '', 'Active', 'Inactive'.
     * @returns {Bluebird<void>}
     */
    public sendEvent({ event, timestamp, state }: {
        event: BaseEvent, timestamp?: Date, state?: string
    }) {
        const queryString: { [key: string]: any } = event.formatQueryString();
        if (timestamp !== undefined) {
            queryString.timestamp = timestamp;
        }
        if (state !== undefined) {
            queryString.state = state;
        }
        return this.getWrapper('/api/createEvent', queryString).then((res: any) => {
            logging.info(`Event with ${JSON.stringify(queryString)} was sent to ${this.systemUrl}`);
            logging.info(res);
        }, (err: any) => {
            logging.error(err);
        });
    }

    /**
     * Gets all of the names, emails, and ids of users in the system.
     * @returns {Bluebird<any>}
     */
    public getUsers() {
        return this.getWrapper('/ec2/getUsers')
                .then((users: any) => {
                    return users.map((user: { [key: string]: string }) => {
                        return { name: user.name, email: user.email, id: user.id };
                    });
                }, (err: any) => {
                    logging.error(err);
                    return err.name;
                });
    }

    /**
     * Removes a user from the system by a user's id.
     * @param {string} userid
     * @returns {Bluebird<void>}
     */
    public removeUser(userid: string) {
        return this.postWrapper('/ec2/removeUser', { id: userid })
                .then((res: any) => {
                    logging.info('Success', res);
                }, (err: any) => {
                    logging.error(err);
                });
    }

    /**
     * A GET request wrapper.
     * @param {string} target
     * @param {object} qs
     * @returns {Bluebird<any>}
     */
    protected getWrapper(target: string, qs?: object) {
        return request({
            auth: {
                pass: this.password,
                sendImmediately: false,
                user: this.user
            },
            qs: (qs),
            rejectUnauthorized: false,
            url: this.systemUrl + target
        }).then(this.makeObject);
    }

    /**
     * A POST request wrapper.
     * @param {string} target
     * @param {object} data
     * @returns {Bluebird<any>}
     */
    protected postWrapper(target: string, data?: object) {
        return request({
            auth: {
                pass: this.password,
                sendImmediately: false,
                user: this.user
            },
            json: data,
            method: 'POST',
            rejectUnauthorized: false,
            url: this.systemUrl + target
        }).then(this.makeObject);
    }

    /**
     * Converts all of the system's responses to json.
     * @param response
     * @returns {any}
     */
    private makeObject(response: any) {
        return typeof(response) === 'string' ? JSON.parse(response) : response;
    }
}
