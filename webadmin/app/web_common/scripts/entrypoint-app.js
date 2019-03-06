import 'ngstorage';
import 'screenfull';
import 'bootstrap-sass';
import 'angular-ui-bootstrap';
import 'angular-clipboard';
import 'jquery-mousewheel';

// Polyfill
require('es6-promise/auto');

//Vendor scripts
require('./vendor/client-detection.js');
require('./vendor/date.js');
require('./vendor/hls.js');
require('./vendor/Impetus.js');
require('./vendor/cast.js');
require('./vendor/cast-framework.js');
require('./vendor/cast-sender.js');

require('./../components/flashls.js');
require('./../components/jshls.js');
require('./../components/nativeplayer.js');

//App
require('./module.js');

//Directives
require('./directives/cameraLinks.js');
require('./directives/cameraPanel.js');
require('./directives/cameraStatus.js');
require('./directives/cameraViewInformer.js');
require('./directives/copyButton.js');
require('./directives/icon.js');
require('./directives/placeholder.js');
require('./directives/previewImage.js');
require('./directives/timeline.js');
require('./directives/videowindow.js');
require('./directives/volumeControl.js');
require('./directives/webclient.js');

//Filters
require('./filters/escape.js');
require('./filters/highlight.js');

//Services
require('./services/animateScope.js');
require('./services/cameraRecords.js');
require('./services/camerasProvider.js');
require('./services/chromecast.js');
require('./services/nativeClient.js');
require('./services/page.js');
require('./services/poll.js');
require('./services/systemAPI.js');
require('./services/timelineActions.js');
require('./services/timelineCanvasRender.js');
require('./services/timelineConfig.js');
require('./services/timelineModels.js');
require('./services/voiceControl.js');

//Controllers
require('./controllers/view.js');
