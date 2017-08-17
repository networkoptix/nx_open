angular.module('nxCommon')
	.directive('copyButton', ['$timeout', function ($timeout) {
		return{
			restrict: 'E',
        	scope:{
        	},
        	templateUrl: Config.viewsDirCommon + 'components/copyButton.html',
        	link: function(scope){
        		scope.buttonText = "Copy";

        		function resetButton(){
        			scope.buttonText = "Copy";
        		}

        		scope.alertClick = function(){
        			scope.buttonText = "Link Copied";
        			$timeout(function(){resetButton();}, 3000);
        		};
        	}
		};
	}]);