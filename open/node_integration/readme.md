<!--
 Copyright 2017-present Network Optix, Inc.
 This source code file is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, you can obtain one at http://mozilla.org/MPL/2.0/.
-->
# Nx Node.js Integration

A framework that allows developers and integrators to quickly and easily make integrations
    with Nx Witness Server.

## Features
- Send Events to the System: Automate sending generic Events
- Create new actions for the System: Create middleware to receive HTTP actions
- Manage rules: Automate generating or editing Event rules and Soft triggers
- Make API calls: A framework to add more API calls

## Examples
### Quick explanation of the class we are going to use.
* GenericEvent - Sends a Generic Event to the Server.
* NodeServer - Listens for HTTP requests and executes callback functions for
    NodeHttpActions.
* NodeHttpAction - Creates a rule on the Server that sends an HTTP action to the
    NodeServer and does some user defined callback.
* Rule - Helps combine Event and Action classes in a way that simplifies the api call for
    making a rule on the Server.
* SoftTrigger - Creates a soft trigger event for a rule on the Server.
* System - Interacts with the VMS Server api to get information and create objects in the
    Server.

### How to connect to the VMS Server using the System class.  
```typescript
import { System } from 'nx-node-integration';
const config = require('./nodeConfig');
const server: System = new System(config.systemUrl,
                                  config.username,
                                  config.password,
                                  config.rules);
```
**Note: The following examples assume that the server object has been created.**

### Sending a Generic Event to the Server using the GenericEvent class. 
```typescript
import { GenericEvent } from 'nx-node-integration';
const genericEvent: GenericEvent = new GenericEvent('Node.js', 'Quick example.');
server.sendEvent({ event: genericEvent });
```

### Creating a rule where clicking on a soft trigger shows a notification.
```typescript
import { HttpAction, Rule, ShowPopup, SoftTrigger } from 'nx-node-integration';
const softTrigger: SoftTrigger = new SoftTrigger('Send Generic Event');
const showNotification: ShowPopup = new ShowPopup();
const rule1: Rule = new Rule('Soft Trigger creates a notification in the System.', false)
    .on(softTrigger)
    .do(showNotification);
server.saveRuleToSystem(rule1).then((rule: Rule | null) => {
    console.log(`Rule created on the server with id: ${rule.ruleId}`);
});
```
### Creating a soft trigger that executes a nodejs callback
This example execute code creates a rule that will send an http action to the NodeServer
   when a user presses the soft trigger created by this script.
   When the NodeServer receives the http action the callback sends a Generic Event to the
   Server.
```typescript
import { 
    GenericEvent, NodeHttpAction, NodeServer, Rule, SoftTrigger
} from 'nx-node-integration';
const nodeServer: NodeServer = new NodeServer(config.myIp, config.myPort);
const genericEvent: GenericEvent = new GenericEvent('Node.js', 'Callback was clicked');
const softTrigger: SoftTrigger = new SoftTrigger('Fire node callback', '_lock_locked');
const nodeCallback: NodeHttpAction = new NodeHttpAction();
nodeCallback.configDefaultHandler('example', () => {
    server.sendEvent({event: genericEvent});
});
const rule1: Rule = new Rule('Soft trigger that makes the express server send a ' +
 'Generic Event')
    .on(softTrigger)
    .do(nodeCallback);
server.saveRuleToSystem(rule1).then((rule: Rule | null) => {
    console.log(`Rule created on the server with id: ${rule.ruleId}`);
});
```

**Note: We are not saving the rules anywhere. As a result of this the rules will be
    recreated every time the script is run.**

### How to prevent rules from being created every time the script is run.
```typescript
// Saves the rule's comment/name and id to the config file.
const saveRuleToConfig = (rule: Rule | null) => {
    if (rule === null) {
        return;
    }
    config.rules[rule.comment] = rule.ruleId;
    fs.writeFileSync(path.resolve(__dirname, './nodeConfig.json'),
            JSON.stringify(config, undefined, 4));
};

const exRule = new Rule('Test rule to save');
server.saveRuleToSystem(exRule).then((rule: Rule | null) => {
    saveRuleToConfig(rule);
});
```

### How to get other information from the Server
In this example the server object will get all cameras, rules, and users from the System.
```typescript
import { Logger, System } from 'nx-node-integration';

const logging = Logger.getLogger('Other Server functions');
const config = require('./nodeConfig');
const server: System = new System(config.systemUrl,
                                  config.username,
                                  config.password,
                                  config.rules);
server.getCameras().then((cameras) => {
    cameras.forEach((camera) => {
        logging.info(`${camera.name}'s id is ${camera.id}`);
    });
});

server.getRules().then((rules) => {
    rules.forEach((rule) => {
        logging.info(rule);
    });
});
server.getUsers().then((users) => {
    users.forEach((user) => {
        logging.info(`${user.name}'s email is ${user.email}`);
    })
})
```

## Getting Started

### Prerequisites
Required Versions
* node.js: 11.3 or newer
* typescript: 3.2.4 or newer
* Nx Witness: 4.0.0 beta or newer

Install node.js, npm, and typescript,
1) Install node.js on your machine https://nodejs.org/en/download/.
2) Run 'npm install npm@latest -g' to update npm.
3) Run 'npm install typescript -g'.

Install Nx Witness or other Powered-by-Nx product, version 4.0 beta or newer.  
To get latest beta version - apply for Developer Early Access Program here:
    http://www.networkoptix.com/develop-with-meta/

### Quick Start
1) Run 'npm install'.
2) Go to the examples directory
3) Run 'tsc *.ts'
4) Edit the nodeConfig.json file with the following. **Note: Leave the rules section
    empty, the scripts will fill it in automatically.**
  ```json
   {
       "systemUrl": "{{Ip address and port of your System running the VMS Server}}",
       "myIp": "{{The Ip address of the machine running this script}}",
       "myPort": "{{The port you want the node.js express server to listen to}}",
       "username": "{{The user name of an account with at least administrator level permissions}}",
       "password": "{{The password for this account from the previous line}}",
       "rules": {}
   }
   ```
   Example nodeConfig.json
  ```json
   {
       "systemUrl": "0.0.0.0:7001",
       "myIp": "0.0.0.0",
       "myPort": "3000",
       "username": "admin",
       "password": "password1234",
       "rules": {}
   }
   ```
5) Pick an example js file to run from the examples directory. Then, run the example using
    'node {{file}}.js'.

  ```
   node softTriggerHttpAction.js
   ```
6) Open the NxWitness desktop client.
7) Open a camera on the grid. There should be a soft trigger called "Node callback -
    simple".
8) Press the soft trigger and you will see a log message in the terminal that says
    "Callback works".

## Authors

**Network Optix**

## License
This project is licensed under the [Mozilla Public License, v. 2.0](
http://mozilla.org/MPL/2.0/) - see the [LICENSE.md]() file for details.

