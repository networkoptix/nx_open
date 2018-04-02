"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const testing_1 = require("@angular/core/testing");
const release_component_1 = require("./release.component");
describe('App', () => {
    beforeEach(() => {
        testing_1.TestBed.configureTestingModule({
            declarations: [release_component_1.ReleaseComponent],
            providers: [{ provide: 'barService', useValue: { getData: () => { } } }]
        });
    });
    it('should work', () => {
        let fixture = testing_1.TestBed.createComponent(release_component_1.ReleaseComponent);
        expect(fixture.componentInstance instanceof release_component_1.ReleaseComponent).toBe(true, 'should create ReleaseComponent');
    });
});
//# sourceMappingURL=release.component.spec.js.map