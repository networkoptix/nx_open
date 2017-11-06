'use strict';

angular.module('cloudApp')
    .factory('notifyWatcher', function () {
        return {
            systemToRemove: null,
            removeSystem: function(system){
                this.systemToRemove = system;
            }
        };
    });