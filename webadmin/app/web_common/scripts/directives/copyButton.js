angular.module('nxCommon')
	.directive('copyButton', ['$timeout', function ($timeout) {
		return{
			restrict: 'E',
        	scope:{
                copyTitle: '=',
                copyData: '='
        	},
        	templateUrl: Config.viewsDirCommon + 'components/copyButton.html',
        	link: function(scope){
        		scope.copyText = "Copy";

        		function resetButton(){
        			scope.copyText = "Copy";
        		}

        		scope.alertClick = function(){
        			scope.copyText = "Link Copied";
        			$timeout(function(){resetButton();}, 3000);
        		};
        	}
		};
	}]);