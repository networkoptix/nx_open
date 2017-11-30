'use strict';


angular.module('webadminApp').directive('fileupload', ['dialogs', function(dialogs){
    return {
        restrict: 'E',
        template :
            '<span class=\'btn btn-success fileinput-button\' ><span>{{text}}</span>' +
            '<input type=\'file\' name=\'installerFile1\' class=\'fileupload\' id=\'fileupload\' >' +
            '</span>' +
            '<div class=\'progress\' style=\'display:none;\'><div class=\'progress-bar progress-bar-success progress-bar-striped\'></div></div>' ,
        link :  function (scope, element, attrs) {
            element.find('.fileupload').fileupload({
                url: attrs.url,
                dataType: 'json',
                done: function (e, data) {
                    if(data.result.error==='0'){
                        dialogs.alert(L.fileUpload.started);

                        //call restart?
                    }else{
                        switch(data.result.errorString){
                            case 'UP_TO_DATE':
                                dialogs.alert(L.fileUpload.upToDate);
                                break;
                            case 'INVALID_FILE':
                                dialogs.alert(L.fileUpload.invalidFile);
                                break;
                            case 'INCOMPATIBLE_SYSTEM':
                                dialogs.alert(L.fileUpload.incompatibleSystem);
                                break;
                            case 'EXTRACTION_ERROR':
                                dialogs.alert(L.fileUpload.extractionError);
                                break;
                            case 'INSTALLATION_ERROR':
                                dialogs.alert(L.fileUpload.installationError);
                                break;
                        }
                    }
                    element.find('.progress').hide();
                },
                progressall: function (e, data) {
                    var progress = parseInt(data.loaded / data.total * 100, 10);
                    element.find('.progress').show();
                    element.find('.progress-bar').css(
                        'width',
                            progress + '%'
                    );
                }
            }).prop('disabled', !$.support.fileInput)
            .parent().addClass($.support.fileInput ? undefined : 'disabled');

            scope.text = attrs.text || L.fileUpload.selectFiles;
        }
    };
}]);
