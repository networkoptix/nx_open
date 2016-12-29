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
  }
});