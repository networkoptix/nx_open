"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const testing_1 = require("@angular/core/testing");
const download_component_1 = require("./download.component");
describe('App', () => {
    beforeEach(() => {
        testing_1.TestBed.configureTestingModule({
            declarations: [download_component_1.DownloadComponent],
            providers: [{ provide: 'barService', useValue: { getData: () => { } } }]
        });
    });
    it('should work', () => {
        let fixture = testing_1.TestBed.createComponent(download_component_1.DownloadComponent);
        expect(fixture.componentInstance instanceof download_component_1.DownloadComponent).toBe(true, 'should create DownloadComponent');
    });
});
//# sourceMappingURL=download.component.spec.js.map