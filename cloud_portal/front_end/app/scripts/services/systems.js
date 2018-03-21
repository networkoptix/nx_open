"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const _ = require("underscore");
const angular = require("angular");
(function () {
    'use strict';
    angular
        .module('cloudApp')
        .service('systemsProvider', ['cloudApi', '$interval', '$q', 'CONFIG', 'languageService',
        function (cloudApi, $interval, $q, CONFIG, languageService) {
            const self = this;
            this.systems = [];
            this.forceUpdateSystems = function () {
                return cloudApi.systems().then(function (result) {
                    self.systems = self.sortSystems(result.data);
                });
            };
            this.delayedUpdateSystems = function () {
                $interval(this.forceUpdateSystems, CONFIG.updateInterval);
            };
            this.getSystem = function (systemId) {
                const system = this.systems.find(function (system) {
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
                let preSort = _.sortBy(systems, function (system) {
                    return self.getSystemOwnerName(system, currentUserEmail, true);
                });
                // Sort by usage frequency is more important than Alphabet
                return _.sortBy(preSort, function (system) {
                    return -system.usageFrequency;
                });
            };
            this.getMySystems = function (currentUserEmail, currentSystemId) {
                return this.systems.filter(function (system) {
                    return system.ownerAccountEmail == currentUserEmail && system.id != currentSystemId;
                });
            };
            this.forceUpdateSystems();
            this.delayedUpdateSystems();
        }]);
})();
//# sourceMappingURL=systems.js.map