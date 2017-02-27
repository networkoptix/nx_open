'use strict';
angular.module('webadminApp')
    .directive('languageSelect', ['mediaserver',
    function (mediaserver) {
        return {
            restrict: 'EA',
            templateUrl: Config.viewsDir + 'components/languageSelect.html',
            scope:{
            },
            link:function(scope, element, attrs, ngModel){
                console.log("init language select");
                mediaserver.getLanguages().then(function(languages){
                    console.log("got languages", languages);
                    scope.activeLanguage = _.find(languages, function(lang){
                        return lang.language == L.language;
                    });
                    if(!scope.activeLanguage){
                        scope.activeLanguage = languages[0];
                    }
                    scope.languages = languages;
                });
                scope.changeLanguage = function(language){
                    if(language == L.language){
                        return;
                    }
                    cloudApi.changeLanguage(language).then(function(){
                        window.location.reload();
                    });
                }
            }
        };
    }]);