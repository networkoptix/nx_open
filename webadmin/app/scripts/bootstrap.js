'use strict';

/**
    This module prepares to run a bootstrap application: detects language, requests language strings
*/

window.L = {};

window.setLanguage = function(lang){

    function setCookie(cname, cvalue, exdays) {
        var d = new Date();
        d.setTime(d.getTime() + (exdays*24*60*60*1000));
        var expires = "expires="+ d.toUTCString();
        document.cookie = cname + "=" + cvalue + ";" + expires + ";path=/";
    }

    setCookie("language", lang, 100); // Almost never expiring cookie
};
setTimeout(function LanguageDetect(){

    function getCookie(cname) {
        var name = cname + "=";
        var ca = document.cookie.split(';');
        for(var i = 0; i < ca.length; i++) {
            var c = ca[i];
            while (c.charAt(0) == ' ') {
                c = c.substring(1);
            }
            if (c.indexOf(name) == 0) {
                return c.substring(name.length, c.length);
            }
        }
        return "";
    }

    var userLang = getCookie("language");
    if(!userLang) {
        var match = window.location.href.match(/[?&]lang=([^&#]+)/i);
        if(match){
            userLang = match[1];
        }
    }
    if(!userLang){
        userLang = navigator.language || navigator.userLanguage;
        userLang = userLang.replace('-','_');
        userLang = _.find(Config.supportedLanguages, function(supportedLanguage){
            return supportedLanguage.indexOf(userLang) == 0;
        });
    }
    if(!userLang || Config.supportedLanguages.indexOf(userLang)<0){
        // We weren't able to detect language or detected language is not supported in this customization
        userLang = Config.defaultLanguage;
    }

    // Get request
    $.ajax({
        url: 'lang_' + userLang + '/language.json',
        async: true,
        dataType: 'json',
        success: function (response) {
            L = response;// Fill global L variable
            Config.viewsDir = 'lang_' + L.language + '/views/';
            Config.viewsDirCommon =  'lang_' + L.language + '/web_common/views/';
            angular.bootstrap(document, ['webadminApp']);
        },
        error:function(){
            // No selected language - fallback to default, which is mandatory
            $.ajax({
                url: 'lang_' + Config.defaultLanguage + '/language.json',
                async: true,
                dataType: 'json',
                success: function (response) {
                    L = response;// Fill global L variable
                    Config.viewsDir = 'lang_' + Config.defaultLanguage + '/views/';
                    Config.viewsDirCommon =  'lang_' + Config.defaultLanguage + '/web_common/views/';
                    angular.bootstrap(document, ['webadminApp']);
                },
                error:function(){
                    Config.viewsDir = 'views/';
                    // Can't get default language.json - fallback to
                    $.ajax({
                        url: 'language.json',
                        async: true,
                        dataType: 'json',
                        success: function (response) {
                            L = response;// Fill global L variable

                            $.ajax({
                                url: 'web_common/commonLanguage.json',
                                async: true,
                                dataType: 'json',
                                success: function (response) {
                                    L.common = response;// Fill global L variable
                                    angular.bootstrap(document, ['webadminApp']);
                                },
                                error:function(){
                                    // Never should've happened
                                    // Try loading anyways
                                    angular.bootstrap(document, ['webadminApp']);
                                    console.error("Can't get commonLanguage.json");
                                }
                            });
                        },
                        error:function(){
                            // Never should've happened
                            // Try loading anyways
                            angular.bootstrap(document, ['webadminApp']);
                            console.error("Can't get language.json");
                        }
                    });
                }
            });
        }
    });
});