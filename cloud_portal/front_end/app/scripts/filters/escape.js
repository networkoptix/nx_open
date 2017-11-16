'use strict';

angular.module('cloudApp')
.filter('escape', function($sce) {
  var entityMap = {
    '&': '&amp;',
    '<': '&lt;',
    '>': '&gt;',
    '"': '&quot;',
    "'": '&#39;',
    '/': '&#x2F;',
    '`': '&#x60;',
    '=': '&#x3D;'
  };
  return function(text, phrase) {
    return String(text).replace(/[&<>"'`=\/]/g, function (s) {
      return entityMap[s];
    });
  }
});