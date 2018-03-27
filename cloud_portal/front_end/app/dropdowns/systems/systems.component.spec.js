"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const testing_1 = require("@angular/core/testing");
const systems_component_1 = require("./systems.component");
describe('NxActiveSystemDropdown', () => {
    let component;
    let fixture;
    beforeEach(testing_1.async(() => {
        testing_1.TestBed
            .configureTestingModule({
            declarations: [systems_component_1.NxSystemsDropdown]
        })
            .compileComponents();
    }));
    beforeEach(() => {
        fixture = testing_1.TestBed.createComponent(systems_component_1.NxSystemsDropdown);
        component = fixture.componentInstance;
        fixture.detectChanges();
    });
    it('should create', () => {
        expect(component).toBeTruthy();
    });
});
//# sourceMappingURL=systems.component.spec.js.map