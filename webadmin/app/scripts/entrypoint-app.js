import 'ngstorage';
import 'angular-route';
import 'bootstrap-sass';
import 'angular-resource';
import 'angular-sanitize';
import 'angular-ui-bootstrap';
import 'angular-clipboard';
import 'tc-angular-chartjs';

import 'hint.css/hint.min.css';
import 'rangeslider.js/dist/rangeslider.css';
import '../styles/main.scss';

require('es6-promise/auto');
require('./config.js');
require('./bootstrap.js');

//App
require('./app.js');

//Vendor
require('./vendor/vkbeautify.js');

//Services
require('./services/cloudAPI.js');
require('./services/dialogs.js');
require('./services/mediaserver.js');
require('./services/visualLog.js');

//Directives
require('./directives/autoFillSync.js');
require('./directives/autofocus.js');
require('./directives/debug.js');
require('./directives/disableAutocomplete.js');
require('./directives/languageSelect.js');
require('./directives/navbar.js');
require('./directives/networkSettings.js');
require('./directives/passwordInput.js');
require('./directives/pwcheck.js');
require('./directives/systemInfo.js');
require('./directives/validateEmail.js');
require('./directives/validateField.js');

//Controllers
require('./controllers/dialogs/cloudDialog.js');
require('./controllers/dialogs/setup.js');

require('./controllers/advanced.js');
require('./controllers/metrics.js');
require('./controllers/healthReport.js');
require('./controllers/apitool.js');
require('./controllers/client.js');
require('./controllers/debug.js');
require('./controllers/devtools.js');
require('./controllers/health.js');
require('./controllers/info.js');
require('./controllers/join.js');
require('./controllers/login.js');
require('./controllers/main.js');
require('./controllers/navigation.js');
require('./controllers/offline.js');
require('./controllers/restart.js');
require('./controllers/sdkeula.js');
require('./controllers/settings.js');
require('./controllers/serverdoc.js');
