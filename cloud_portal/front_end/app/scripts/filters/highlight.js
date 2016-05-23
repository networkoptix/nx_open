'use strict';

angular.module('cloudApp')
  .filter('highlight', function($sce) {
    return function(text, phrase) {
      if (phrase && phrase!==''){
        text = text.replace(new RegExp('('+phrase+')', 'gi'), '<b>$1</b>')
      }

      return $sce.trustAsHtml(text)
    }
  })