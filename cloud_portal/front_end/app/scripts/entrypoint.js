import 'angular';
import 'ng-toast';
import 'ngstorage';
import 'angular-route';
import 'angular-base64';
import 'angular-cookies';
import 'angular-resource';
import 'angular-sanitize';
import '@ng-bootstrap/ng-bootstrap';
import 'angular-clipboard';
import 'jquery-mousewheel';
import 'what-input';

import 'ng-toast/dist/ngToast.css';
import 'ng-toast/dist/ngToast-animations.css';
import '../styles/main.scss';
import '../app.component.scss';

require('./client-detection.js');

//Vendor
require('./vendor/protocolcheck.js');

//App
require('./app.js');
require('./downgraded-providers.ts');

//Directives
require('./directives/autofocus.js');
require('./directives/footer.js');
require('./directives/header.js');
require('./directives/openClientButton.js');
require('./directives/passwordInput.js');
require('./directives/process.js');
require('./directives/setTitle.js');
require('./directives/validateEmail.js');
require('./directives/validateField.js');
require('./directives/validateCommonPassword.js');
require('./directives/validateWeakPassword.js');

//Filters
require('./filters/escape.js');

//Services
require('./services/account.ts');
require('./services/angular-uuid2.ts');
require('./services/cloud_api.ts');
require('./services/dialogs.ts');
require('./services/language.ts');
require('./services/mediaserver.js');
require('./services/page.js');
require('./services/poll.js');
require('./services/process.ts');
require('./services/system.ts');
require('./services/systems.ts');
require('./services/urlProtocol.js');
require('./services/authorizationCheckService.ts');
require('./services/location-proxy.ts');

//Controllers
require('./controllers/account.js');
require('./controllers/activateRestore.js');
require('./controllers/debug.js');
require('./controllers/register.js');
require('./controllers/startPage.js');
require('./controllers/static.js');
require('./controllers/system.js');
require('./controllers/systems.js');
require('./controllers/view.js');
