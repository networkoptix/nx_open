import 'ngstorage';
import 'bootstrap-sass';
import 'angular-ui-bootstrap';

//Vendor scripts
require('./vendor/client-detection.js');

//App
require('./module.js');

//Services
require('./services/nativeClient.js');
require('./services/page.js');
require('./services/poll.js');
require('./services/systemAPI.js');
