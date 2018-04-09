"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const testing_1 = require("@angular/core/testing");
const dialogs_service_1 = require("./dialogs.service");
describe('nxDialogsService', () => {
    beforeEach(() => {
        testing_1.TestBed.configureTestingModule({
            providers: [dialogs_service_1.nxDialogsService]
        });
    });
    it('should be created', testing_1.inject([dialogs_service_1.nxDialogsService], (service) => {
        expect(service).toBeTruthy();
    }));
});
//# sourceMappingURL=dialogs.service.spec.js.map