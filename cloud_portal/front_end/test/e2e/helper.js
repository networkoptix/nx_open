'use strict';

var Helper = function () {

    this.baseEmail = 'noptixqa@gmail.com';

    // Get valid email with random number between 100 and 1000
    this.getRandomEmail = function() {
        var randomNumber = Math.floor((Math.random() * 10000)+1000); // Random number between 1000 and 10000
        return 'noptixqa+' + randomNumber + '@gmail.com';
    };
    this.userEmailExisting = 'noptixqa+1@gmail.com'; // valid existing email
    this.userEmail1 = 'noptixqa+1@gmail.com';
    this.userEmail2 = 'noptixqa+2@gmail.com';
    this.userEmailWrong = 'nonexistingperson@gmail.com';

    this.userFirstName = 'TestFirstName';
    this.userLastName = 'TestLastName';
    this.userPassword = 'qweasd123';
    this.userPasswordNew = 'qweasd123qwe';
    this.userPasswordWrong = 'qweqwe123';

    this.userNameCyrillic = 'Кенгшщзх';
    this.userNameSmile = '☠☿☂⊗⅓∠∩λ℘웃♞⊀☻★';
    this.userNameHierog = '您都可以享受源源不絕的好禮及優惠';

    this.userPasswordCyrillic = 'йцуфывячс';
    this.userPasswordSmile = '☠☿☂⊗⅓∠∩λ℘웃♞⊀☻★';
    this.userPasswordHierog = '您都可以享受源源不絕的好禮及優惠';
    this.userPasswordWrong = 'qweqwe123';

    this.login = function(email, password) {

        var loginButton = element(by.linkText('Login'));
        var loginDialog = element(by.css('.modal-dialog'));
        var emailInput = loginDialog.element(by.model('auth.email'));
        var passwordInput = loginDialog.element(by.model('auth.password'));
        var dialogLoginButton = loginDialog.element(by.buttonText('Login'));
        var loginSuccessElement = element.all(by.css('.auth-visible')).first(); // some element on page, that is only visible when user is authenticated


        browser.get('/');
        browser.waitForAngular();

        loginButton.click();

        emailInput.sendKeys(email);
        passwordInput.sendKeys(password);

        dialogLoginButton.click();
        browser.sleep(2000); // such a shame, but I can't solve it right now

        // Check that element that is visible only for authorized user is displayed on page
        expect(loginSuccessElement.isDisplayed()).toBe(true);
    };

    this.logout = function() {
        var navbar = element(by.css('header')).element(by.css('.navbar'));
        var userAccountDropdownToggle = navbar.element(by.css('a[uib-dropdown-toggle]'));
        var userAccountDropdownMenu = navbar.element(by.css('[uib-dropdown-menu]'));
        var logoutLink = userAccountDropdownMenu.element(by.linkText('Logout'));
        var loginSuccessElement = element.all(by.css('.auth-visible')).first(); // some element on page, that is only visible when user is authenticated

        expect(userAccountDropdownToggle.isDisplayed()).toBe(true);

        userAccountDropdownToggle.click();
        logoutLink.click();
        browser.sleep(500); // such a shame, but I can't solve it right now

        // Check that element that is visible only for authorized user is NOT displayed on page
        expect(loginSuccessElement.isDisplayed()).toBe(false);
    };

    this.getEmailTo = function(emailAddress) {
        var deferred = protractor.promise.defer();
        console.log("Waiting for an email...");

        notifier.on("mail", function(mail){
            if(emailAddress === mail.headers.to){
                console.log("Catch email to: " + mail.headers.to);
                deferred.fulfill(mail);
                notifier.stop();
                return;
            }
            console.log("Ignore email to: " + mail.headers.to);
        });

        notifier.start();

        return deferred.promise;
    };
};

module.exports = Helper;