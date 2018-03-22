"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const testing_1 = require("@angular/core/testing");
const account_settings_component_1 = require("./account-settings.component");
describe('NxAccountSettingsDropdown', () => {
    let component;
    let fixture;
    beforeEach(testing_1.async(() => {
        testing_1.TestBed
            .configureTestingModule({
            declarations: [account_settings_component_1.NxAccountSettingsDropdown]
        })
            .compileComponents();
    }));
    beforeEach(() => {
        fixture = testing_1.TestBed.createComponent(account_settings_component_1.NxAccountSettingsDropdown);
        component = fixture.componentInstance;
        fixture.detectChanges();
    });
    it('should create', () => {
        expect(component).toBeTruthy();
    });
});
//# sourceMappingURL=account-settings.component.spec.js.map