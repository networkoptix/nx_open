'use strict';

angular.module('webadminApp')
    .directive('volumeControl', function () {
        return{
        	restrict: 'E',
        	scope:{
        		volumeLevel: "="
        	},
        	templateUrl: Config.viewsDir + 'components/volumeControl.html',
        	link: function(scope, element){
		        var selector = '[data-rangeslider]';
        		$('volume-control>.bar').rangeslider({
		        	// Deactivate the feature detection
		        	polyfill: false,
		        	// Callback function
		            onInit: function() {
		            	this.$element[0].value = scope.volumeLevel;
		            },
		            // Callback function
		            onSlide: function(position, value) {
		            	if(isNaN(value))
		            		return;
		                //console.log("In VolumeControl", new Date());
		                scope.volumeLevel = value;
		                scope.$apply();
		            },
		            // Callback function
		            onSlideEnd: function(position, value) {
		            	if(isNaN(value))
		            		return;
		                //console.log("In Volume Control", new Date());
		                scope.volumeLevel = value;
		                scope.$apply();
		            }
		        });
        	}
        };
    });