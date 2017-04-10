'use strict';

angular.module('webadminApp')
    .directive('volumeControl', function () {
        return{
        	restrict: 'E',
        	scope:{
        		volumeLevel: "&"
        	},
        	link: function(scope, element){
        		//window.alert("directive loaded");
        		element.rangeslider({
		        	// Deactivate the feature detection
		        	polyfill: false,
		        	// Callback function
		            onInit: function() {
		                console.log(this.$element[0]);
		            },
		            // Callback function
		            onSlide: function(position, value) {
		                console.log('onSlide');
		                console.log('position: ' + position, 'value: ' + value);
		            },
		            // Callback function
		            onSlideEnd: function(position, value) {
		                console.log('onSlideEnd');
		                console.log('position: ' + position, 'value: ' + value);
		            }
		        });
        	}
        };
    });