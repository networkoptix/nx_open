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
        		//window.alert("directive loaded");
        		function valueOutput(element) {
		            var value = element.value;
		            var output = element.parentNode.getElementsByTagName('output')[0] || element.parentNode.parentNode.getElementsByTagName('output')[0];
		            output.innerText = value;
		        }
		        var selector = '[data-rangeslider]';
        		$('volume-control>.bar').rangeslider({
		        	// Deactivate the feature detection
		        	polyfill: false,
		        	// Callback function
		            onInit: function() {
		            	this.$element[0].value = scope.volumeLevel;
		                valueOutput(this.$element[0]);
		            },
		            // Callback function
		            onSlide: function(position, value) {
		            	if(isNaN(value))
		            		return;
		                valueOutput(this.$element[0]);
		                scope.volumeLevel = value;
		            },
		            // Callback function
		            onSlideEnd: function(position, value) {
		            	if(isNaN(value))
		            		return;
		                valueOutput(this.$element[0]);
		                scope.volumeLevel = value;
		            }
		        });
        	}
        };
    });