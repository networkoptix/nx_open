var webpackConfig = require('./webpack.coverage');

module.exports = function (config) {
    var _config = {
        basePath: '',
        
        frameworks: ['jasmine'],
        
        files: [
            {pattern: './karma-test-shim.js', watched: false}
        ],
        
        preprocessors: {
            './karma-test-shim.js': ['webpack', 'sourcemap']
        },
        
        webpack: webpackConfig,
        
        webpackMiddleware: {
            stats: 'errors-only'
        },
        
        webpackServer: {
            noInfo: true
        },
        
        reporters: ['progress', 'kjhtml'],
        port: 9876,
        colors: true,
        logLevel: config.LOG_INFO,
        autoWatch: false,
        browsers: ['Chrome'],
        singleRun: true
    };
    
    config.set(_config);
};
