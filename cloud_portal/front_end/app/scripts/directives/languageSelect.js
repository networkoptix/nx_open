'use strict';
angular.module('cloudApp')
    .directive('languageSelect', ['cloudApi',
    function (cloudApi) {
        return {
            restrict: 'EA',
            templateUrl: Config.viewsDir + 'components/language-select.html',
            scope:{
                system:'='
            },
            link:function(scope, element, attrs, ngModel){
                cloudApi.getLanguages().then(function(data){
                    var languages = data.data;
                    console.log("languages", languages);
                    var activeLanguage = _.find(languages, function(lang){
                        return lang.language == L.language;
                    })
                    scope.activeLanguage = activeLanguage? activeLanguage.name: "LANGUAGE";
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