'use strict';


angular.module('webadminApp')
    .directive('fileupload',function(){
        return {
            restrict: 'E',
                template :
                    '<span class="btn btn-success fileinput-button" ><span>{{text}}</span>' +
                    '<input type="file" name="installerFile1" class="fileupload" id="fileupload" >' +
                    '</span>' +
                    '<div class="progress" style="display:none;"><div class="progress-bar progress-bar-success progress-bar-striped"></div></div>' ,
                link :  function (scope, element, attrs) {
                    element.find('.fileupload').fileupload({
                        url: attrs.url,
                        dataType: 'json',
                        done: function (e, data) {
                            if(data.result.error==0){
                                alert("Updating successfully started. It will take several minutes");

                                //call restart?
                            }else{
                                switch(data.result.errorString){
                                    case 'UP_TO_DATE':
                                        alert("Updating failed. The provided version is already installed.");
                                        break;
                                    case 'INVALID_FILE':
                                        alert("Updating failed. Provided file is not a valid update archive.");
                                        break;
                                    case 'INCOMPATIBLE_SYSTEM':
                                        alert("Updating failed. Provided file is targeted for another system.");
                                        break;
                                    case 'EXTRACTION_ERROR':
                                        alert("Updating failed. Extraction failed, check available storage.");
                                        break;
                                    case 'INSTALLATION_ERROR':
                                        alert("Updating failed. Couldn't execute installation script.");
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

                scope.text = attrs.text || 'Select files';
            }
        }
    });
