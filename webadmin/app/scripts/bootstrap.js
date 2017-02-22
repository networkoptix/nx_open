'use strict';

/**
    This module prepares to run a bootstrap application: detects language, requests language strings
*/

var L = {};
$.ajax({
    url: 'language.json',
    async: true,
    dataType: 'json',
    success: function (response) {
        L = response;// Fill global L variable
        angular.bootstrap(document, ['webadminApp']);
    },
    error:function(){
        // Never should've happened
        // Try loading anyways
        angular.bootstrap(document, ['webadminApp']);
        Console.error("Can't get language.json");
    }
});