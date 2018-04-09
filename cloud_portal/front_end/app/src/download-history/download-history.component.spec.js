"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const testing_1 = require("@angular/core/testing");
const download_history_component_1 = require("./download-history.component");
describe('App', () => {
    beforeEach(() => {
        testing_1.TestBed.configureTestingModule({
            declarations: [download_history_component_1.DownloadHistoryComponent],
            providers: [{ provide: 'barService', useValue: { getData: () => { } } }]
        });
    });
    it('should work', () => {
        let fixture = testing_1.TestBed.createComponent(download_history_component_1.DownloadHistoryComponent);
        expect(fixture.componentInstance instanceof download_history_component_1.DownloadHistoryComponent).toBe(true, 'should create DownloadHistoryComponent');
    });
});
//# sourceMappingURL=download-history.component.spec.js.map