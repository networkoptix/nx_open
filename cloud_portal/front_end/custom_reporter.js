var customReporter = {
    jasmineStarted: function(suiteInfo) {
        console.log('Running suite with ' + suiteInfo.totalSpecsDefined);
    },
    suiteStarted: function(result) {
        console.log('\n' + result.description + ' start\n');
    },
    specStarted: function(result) {
        console.log('\n' + result.fullName);
    },
    specDone: function(result) {
        for(var i = 0; i < result.failedExpectations.length; i++) {
            console.log('Failure: ' + result.failedExpectations[i].message);
            console.log(result.failedExpectations[i].stack);
        }
    },
    suiteDone: function(result) {
        console.log('\nSuite: ' + result.description + ' was ' + result.status);
        for(var i = 0; i < result.failedExpectations.length; i++) {
            console.log('AfterAll ' + result.failedExpectations[i].message);
            console.log(result.failedExpectations[i].stack);
        }
        console.log('\n' + result.description + ' finish\n');
    }
};
module.exports = customReporter;