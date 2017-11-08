'use strict';

angular.module('cloudApp')
    .service('systemsProvider', [ 'cloudApi', '$interval', function (cloudApi, $interval) {
        var self = this;
        this.systems = [];

        this.forceUpdateSystems = function(){
            return cloudApi.systems().then(function(result){
                self.systems = cloudApi.sortSystems(result.data);
            });
        };

        this.delayedUpdateSystems = function(){
            $interval(this.forceUpdateSystems,Config.updateInterval);
        };
        this.forceUpdateSystems();
        this.delayedUpdateSystems();
    }]);