/*
 * Copyright 2017-present Network Optix, Inc.
 * This source code file is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */
import * as express from 'express';
import config from './config';
import { factory } from './logConfig';

const logging = factory.getLogger('Node Server');

/**
 * The NodeServer allows you to target an express server with http actions from the media server.<br>
 * <b>Example</b><br>
 * You can make a function that sends you a text message. Then use a nodeHttpAction that sends a
 * request to this express server and a text message will be sent to you using the function you made.
 * @class
 */
export class NodeServer {
    /**
     * Address is the ip and port of the system you are running the script on.
     */
    public address: string;
    /**
     * Allows the user to send httpActions to the nodeServer and do custom callbacks.
     */
    public httpActionCallbacks: { [key: string]: (query?: any) => void };
    /**
     * App is the express server.
     */
    private app: any;

    /**
     * @param {string} ip The ip address of system running the script.
     * @param {number} port The port of the system running the script.
     * @constructor
     */
    constructor(ip: string, port: number) {
        this.address = `${ip}:${port}`;
        this.httpActionCallbacks = {};
        // Listens for http actions from the server.
        this.app = express();
        this.app.listen(port, () => {
            logging.info(`express has started on port ${port}`);
        });

        // The default callback route, but more can be defined using the addExpressHandler function.
        this.app.use(`/${config.defaultExpressRoute}/:actionKey`,
                (req: express.Request, res: express.Response) => {
            logging.info(`${JSON.stringify(req.params)}`);
            if (req.params.actionKey in this.httpActionCallbacks) {
                return res.send(this.httpActionCallbacks[req.params.actionKey](req.query));
            }
            res.status(404);
            res.send(`${req.params.actionKey} does not exist`);
        });
    }

    /**
     * Adds routes to the express server.
     * @param {string} route The route you want to use. Example: /example
     * @param {(req?: any, res?: any, next?: any) => void} handler Callbacks for the route.
     * @returns {this}
     */
    public addExpressHandler(route: string, handler: (req?: any, res?: any, next?: any) => void) {
        this.app.use(route, handler);
        return this;
    }

    /**
     * Adds the http callback to a route.
     * @param {string} key The key can be anything depending on which route you want it to be
     * handled by.
     * @param {boolean} isDefault States if the callback is being added to default route handler.
     * @param callback Callback function for route.
     */
    public addHttpCallback(key: string, isDefault: boolean, callback?: any) {
        if (callback) {
            if (isDefault) {
                logging.info(`Callback function was added /${config.defaultExpressRoute}/${key}`);
            } else {
                logging.info(`Callback function was added /${key}`);
            }
            this.httpActionCallbacks[key] = callback;
        } else {
            logging.error('No callback was provide');
        }
    }
}
