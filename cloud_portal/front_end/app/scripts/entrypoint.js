import 'angular';
import 'bootstrap';
import 'ng-toast';
import 'ngstorage';
import 'angular-route';
import 'angular-base64';
import 'angular-cookies';
import 'angular-resource';
import 'angular-sanitize';
import 'angular-animate';
import '@ng-bootstrap/ng-bootstrap';
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

//Filters
require('./filters/escape.js');

//Services
require('./services/account.ts');
require('./services/angular-uuid2.ts');
require('./services/cloud_api.ts');
require('./services/dialogs.ts');
require('./services/language.ts');
require('./services/mediaserver.js');
require('./services/nx-config.ts');
require('./services/page.js');
require('./services/poll.js');
require('./services/process.ts');
require('./services/system.js');
require('./services/systems.ts');
require('./services/urlProtocol.js');
require('./services/authorizationCheckService.ts');

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
