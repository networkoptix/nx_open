'use strict';

/**
    This module prepares to run a bootstrap application: detects language, requests language strings
*/

var L = {};
$.ajax({
// url: 'static/views/language.json', 
    url: 'api/utils/language',
    async: true,
    dataType: 'json',
    success: function (response) {
        L = response;// Fill global L variable
        // Fill Config.viewsDir here!
        angular.bootstrap(document, ['cloudApp']);
    },
    error:function(error){
        // Fallback to default language
        $.ajax({
            url: 'static/language.json',
            async: true,
            dataType: 'json',
            success: function (response) {
                L = response;// Fill global L variable
                // Fill Config.viewsDir here!
                angular.bootstrap(document, ['cloudApp']);
            },
            error:function(){
                window.location.href = '503.html'; // Can't retrieve any language - service is unavailable, go to 503
            }
        });
    }
});
