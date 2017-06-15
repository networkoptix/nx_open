'use strict';

angular.module('nxCommon')
	.directive('placeholder',function(){
		return{
			restrict: 'E',
        	scope:{
        	    iconClass: "=",
        	    title: "=",
        	    message: "=",
        	    preloader: "=",
        	    condition: "=ngIf"
        	},
        	templateUrl: Config.viewsDirCommon + 'components/placeholder.html'
		};
	});