'use strict';

angular.module('nxCommon')
    .directive('volumeControl', function () {
        return{
        	restrict: 'E',
        	scope:{
        		volumeLevel: "="
        	},
        	templateUrl: Config.viewsDirCommon + 'components/volumeControl.html',
        	link: function(scope, element){
		        var selector = '[data-rangeslider]';

		        scope.currentVolume = scope.volumeLevel;
		        scope.mute = false;

		        scope.toggleMute = function(){
		   			scope.mute = !scope.mute;
		        	if(scope.mute){
		        		scope.currentVolume = scope.volumeLevel;
		        		scope.volumeLevel = 0;
		        	}
		        	else
		        		scope.volumeLevel = scope.currentVolume;
		        	
		        };

        		$('volume-control>.bar').rangeslider({
		        	// Deactivate the feature detection
		        	polyfill: false,
		        	// Callback function
		            onInit: function() {
		            	this.setPosition(scope.volumeLevel,false);
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
	                	scope.mute = value > 0 ? false : true;
		                scope.$apply();
		            }
		        });
        	}
        };
    });