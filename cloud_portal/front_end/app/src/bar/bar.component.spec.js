"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const testing_1 = require("@angular/core/testing");
const bar_component_1 = require("./bar.component");
describe('App', () => {
    beforeEach(() => {
        testing_1.TestBed.configureTestingModule({
            declarations: [bar_component_1.BarComponent],
            providers: [{ provide: 'barService', useValue: { getData: () => { } } }]
        });
    });
    it('should work', () => {
        let fixture = testing_1.TestBed.createComponent(bar_component_1.BarComponent);
        expect(fixture.componentInstance instanceof bar_component_1.BarComponent).toBe(true, 'should create BarComponent');
    });
});
//# sourceMappingURL=bar.component.spec.js.map