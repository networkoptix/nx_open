'use strict';
var SystemsListPage = require('./po.js');
describe('Systems list suite', function () {

    var p = new SystemsListPage();

    it("should show list of Systems", function () {
        p.get();
        expect("test").toBe("written");
    });
    it("should show Open in NX client button for every online system", function () {
        p.get();
        expect("test").toBe("written");
    });
    it("should оpen in NX client, when clicking 'Open in NX client' button", function () {
        p.get();
        expect("test").toBe("written");
    });
    it("should not open system page, when clicking 'Open in NX client'", function () {
        p.get();
        expect("test").toBe("written");
    });
    it("should not show Open in NX client button for offline system", function () {
        p.get();
        expect("test").toBe("written");
    });
    it("should show system's state for systems (activated, not activated, online, offline)", function () {
        p.get();
        expect("test").toBe("written");
    });
    it("should show system's owner for every system in the list, 'Your system' for user's systems", function () {
        p.get();
        expect("test").toBe("written");
    });
    it("should show email for every system, if system owner's name is empty", function () {
        p.get();
        expect("test").toBe("written");
    });
    it("should opens system page (users list) when clicked on system", function () {
        p.get();
        expect("test").toBe("written");
    });
    it("should update owner name in systems list, if it's changed", function () {
        p.get();
        expect("test").toBe("written");
    });
});