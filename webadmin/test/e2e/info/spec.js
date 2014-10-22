'use strict';
var Page = require('./po.js');
describe('Information Page', function () {

    var p = new Page();

    it("Module",function(){
        expect("all tests").toBe("written");
    });
    return;

    it("should show software info",function(){

        expect("test").toBe("written");
    });

    it("should show storage info",function(){

        expect("test").toBe("written");
    });

    it("health monitoring: should display legend",function(){

        expect("test").toBe("written");
    });
    it("health monitoring: should update graphics ",function(){

        expect("test").toBe("written");
    });
    it("health monitoring: should show message when server is offline",function(){

        expect("test").toBe("written");
    });
    it("health monitoring: should enable-disable indicators using checkboxes",function(){

        expect("test").toBe("written");
    });

    it("log: should show iframe with not-empty date",function(){

        expect("test").toBe("written");
    });

    it("log: should refresh iframe on click on some button",function(){

        expect("test").toBe("written");
    });

    it("log: should show new window on click on some button",function(){

        expect("test").toBe("written");
    });
});