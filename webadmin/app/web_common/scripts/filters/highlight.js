'use strict';

angular.module('nxCommon')
  .filter('highlight', ['$sce',function($sce) {
    return function(text, phrase) {
      if (phrase && phrase!==''){
        text = text.replace(new RegExp('('+phrase+')', 'gi'), '<span class="highlighted">$1</span>')
      }

      return $sce.trustAsHtml(text)
    }
  }]);