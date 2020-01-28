import 'core-js/es6';
import 'core-js/es7/reflect';
import 'core-js/es7/object';   // IE 11 needs Object.entries
import 'core-js/es7/array';   // IE 11 needs includes()

require('zone.js/dist/zone');

if (!Element.prototype.matches) {
  Element.prototype.matches = (<any>Element.prototype).msMatchesSelector ||
          Element.prototype.webkitMatchesSelector;
}

if (process.env.ENV === 'production') {
  // Production
} else {
  // Development and test
  Error['stackTraceLimit'] = Infinity;
  require('zone.js/dist/long-stack-trace-zone');
}
