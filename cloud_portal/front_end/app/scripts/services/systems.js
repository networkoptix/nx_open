"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const _ = require("underscore");
const angular = require("angular");
(function () {
    'use strict';
    angular
        .module('cloudApp')
        .service('systemsProvider', ['cloudApi', '$interval', '$q', '$poll',
        'configService', 'languageService', 'account',
        function (cloudApi, $interval, $q, $poll, configService, languageService, account) {
            const CONFIG = configService.config;
            this.systems = [];
            this.forceUpdateSystems = function () {
                return cloudApi.systems().then((result) => {
                    this.systems = this.sortSystems(result.data);
                });
            };
            this.delayedUpdateSystems = function () {
                this.pollingSystemsUpdate = $poll(() => { return this.forceUpdateSystems(); }, CONFIG.updateInterval);
            };
            this.getSystem = function (systemId) {
                const system = this.systems.find((system) => {
                    return system.id == systemId;
                });
                if (system) {
                    return $q.resolve({ data: [system] });
                }
                else {
                    return cloudApi.systems(systemId);
                }
            };
            this.getSystemOwnerName = function (system, currentUserEmail, forOrder) {
                if (system.ownerAccountEmail == currentUserEmail) {
                    if (forOrder) {
                        return '!!!!!!!'; // Force my systems to be first
                    }
                    return languageService.lang.system.yourSystem;
                }
                if (system.ownerFullName && system.ownerFullName.trim() != '') {
                    return system.ownerFullName;
                }
                return system.ownerAccountEmail;
            };
            this.sortSystems = function (systems, currentUserEmail) {
                // Alphabet sorting
                let preSort = _.sortBy(systems, (system) => {
                    return this.getSystemOwnerName(system, currentUserEmail, true);
                });
                // Sort by usage frequency is more important than Alphabet
                return _.sortBy(preSort, (system) => {
                    return -system.usageFrequency;
                });
            };
            this.getMySystems = function (currentUserEmail, currentSystemId) {
                return this.systems.filter((system) => {
                    return system.ownerAccountEmail == currentUserEmail && system.id != currentSystemId;
                });
            };
            account.checkLoginState()
                .then(() => {
                this.forceUpdateSystems();
                this.delayedUpdateSystems();
            });
        }]);
})();
//# sourceMappingURL=systems.js.map