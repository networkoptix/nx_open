import 'angular';
import 'bootstrap-sass';
import 'ng-toast';
import 'ngstorage';
import 'angular-route';
import 'angular-base64';
import 'angular-cookies';
import 'angular-resource';
import 'angular-sanitize';
import 'angular-animate';
import 'angular-ui-bootstrap';
import 'angular-clipboard';
import 'tc-angular-chartjs';
import 'jquery-mousewheel';

import 'ng-toast/dist/ngToast.css';
import 'ng-toast/dist/ngToast-animations.css';
import '../styles/main.scss';

require('./client-detection.js');

//Vendor
require('./vendor/protocolcheck.js');

//App
require('./app.js');

//Directives
require('./directives/autofocus.js');
require('./directives/footer.js');
require('./directives/header.js');
require('./directives/languageSelect.js');
require('./directives/openClientButton.js');
require('./directives/passwordInput.js');
require('./directives/process.js');
require('./directives/setTitle.js');
require('./directives/validateEmail.js');
require('./directives/validateField.js');

//Filters
require('./filters/escape.js');

//Services
require('./services/account.js');
require('./services/angular-uuid2.js');
require('./services/cloud_api.js');
require('./services/dialogs.js');
require('./services/language.js');
require('./services/mediaserver.js');
require('./services/nx-config.js');
require('./services/page.js');
require('./services/poll.js');
require('./services/process.js');
require('./services/system.js');
require('./services/systems.js');
require('./services/urlProtocol.js');

//Controllers
require('./controllers/account.js');
require('./controllers/activateRestore.js');
require('./controllers/debug.js');
require('./controllers/disconnect.js');
require('./controllers/download.js');
require('./controllers/downloadHistory.js');
require('./controllers/login.js');
require('./controllers/merge.js');
require('./controllers/register.js');
require('./controllers/rename.js');
require('./controllers/share.js');
require('./controllers/startPage.js');
require('./controllers/static.js');
require('./controllers/system.js');
require('./controllers/systems.js');
require('./controllers/view.js');
