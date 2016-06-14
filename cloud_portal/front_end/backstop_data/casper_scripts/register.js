module.exports = function(casper, scenario, vp) {
    // scenario is the current scenario object being run from your backstop config
    // vp is the current viewport object being run from your backstop config

    casper.echo('Typing into password field');
    casper.then(function(){
        casper.page.addCookie({some: 'cookie'});
        this.wait(500);
        this.sendKeys('input[type=password]', 'passwd');
    });
    // Example: Adding script delays to allow for things like CSS transitions to complete.
    casper.wait( 250 );
};