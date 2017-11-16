'use strict';

angular.module('cloudApp')
    .service('systemsProvider', [ 'cloudApi', '$interval', '$q', function (cloudApi, $interval, $q) {
        var self = this;
        this.systems = [];

        this.forceUpdateSystems = function(){
            return cloudApi.systems().then(function(result){
                self.systems = self.sortSystems(result.data);
            });
        };

        this.delayedUpdateSystems = function(){
            $interval(this.forceUpdateSystems,Config.updateInterval);
        };

        this.getSystem = function(systemId){
            var system = _.find(this.systems, function(system){
                return system.id == systemId;
            });

            if(system) { // Cache success
                return $q.resolve({data:[system]});
            }else{ // Cache miss
                return cloudApi.systems(systemId);
            }
        };

        this.getSystemOwnerName = function(system, currentUserEmail, forOrder) {
            if(system.ownerAccountEmail == currentUserEmail ){
                if(forOrder){
                    return '!!!!!!!'; // Force my systems to be first
                }
                return L.system.yourSystem;
            }

            if(system.ownerFullName && system.ownerFullName.trim() != ''){
                return system.ownerFullName;
            }

            return system.ownerAccountEmail;
        };

        this.sortSystems = function (systems, currentUserEmail){
            // Alphabet sorting
            var preSort =  _.sortBy(systems,function(system){
                return self.getSystemOwnerName(system, currentUserEmail, true);
            });
            // Sort by usage frequency is more important than Alphabet
            return _.sortBy(preSort,function(system){
                return -system.usageFrequency;
            });
        };

        this.forceUpdateSystems();
        this.delayedUpdateSystems();
    }]);