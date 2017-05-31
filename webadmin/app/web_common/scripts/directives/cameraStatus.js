'use strict';

angular.module('nxCommon')
	.directive('cameraStatus',function(){
		return{
			restrict: 'E',
        	scope:{
        		camera: "=",
        		type: "="
        	},
        	templateUrl: Config.viewsDirCommon + 'components/cameraStatus.html'
		};
	});