import 'ngstorage';
import 'angular-route';
import 'bootstrap-sass';
import 'angular-resource';
import 'angular-sanitize';
import 'angular-ui-bootstrap';

import 'hint.css/hint.min.css';
import '../styles/main.scss';

require('es6-promise/auto');
require('./config.js');
require('./bootstrap.js');

//App
require('./app-login.js');

//Services
require('./services/dialogs.js');
require('./services/mediaserver.js');

//Directives
require('./directives/navbar.js');

//Controllers
require('./controllers/login.js');
require('./controllers/navigation.js');
