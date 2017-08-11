angular.module('nxCommon')
	.filter('checkProtocol', function(){
		return function(url){
			return url.indexOf('//') >= 0 ? url : "//"+url;
		};
	});