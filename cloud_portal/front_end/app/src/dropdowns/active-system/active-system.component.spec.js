"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const testing_1 = require("@angular/core/testing");
const active_system_component_1 = require("./active-system.component");
describe('NxActiveSystemDropdown', () => {
    let component;
    let fixture;
    beforeEach(testing_1.async(() => {
        testing_1.TestBed
            .configureTestingModule({
            declarations: [active_system_component_1.NxActiveSystemDropdown]
        })
            .compileComponents();
    }));
    beforeEach(() => {
        fixture = testing_1.TestBed.createComponent(active_system_component_1.NxActiveSystemDropdown);
        component = fixture.componentInstance;
        fixture.detectChanges();
    });
    it('should create', () => {
        expect(component).toBeTruthy();
    });
});
//# sourceMappingURL=active-system.component.spec.js.map