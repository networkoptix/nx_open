'use strict';
angular.module('cloudApp')
    .directive('languageSelect', ['cloudApi',
    function (cloudApi) {
        return {
            restrict: 'EA',
            templateUrl: Config.viewsDir + 'components/language-select.html',
            scope:{
                system:'=',
                accountMode:'=',
                ngModel: '='
            },
            link:function(scope, element, attrs, ngModel){
                cloudApi.getLanguages().then(function(data){
                    var languages = data.data;
                    scope.activeLanguage = _.find(languages, function(lang){
                        return lang.language == L.language;
                    });
                    if(!scope.activeLanguage){
                        scope.activeLanguage = languages[0];
                    }
                    scope.languages = languages;
                });
                scope.changeLanguage = function(language){
                    if(!scope.accountMode){
                        if(language == L.language){
                            return;
                        }
                        cloudApi.changeLanguage(language).then(function(){
                            window.location.reload();
                        });
                    }else{
                        scope.ngModel = language;
                        scope.activeLanguage = _.find(scope.languages, function(lang){
                            return lang.language == scope.ngModel;
                        });
                    }
                }
            }
        };
    }]);