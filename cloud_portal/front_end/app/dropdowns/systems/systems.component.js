"use strict";
var __decorate = (this && this.__decorate) || function (decorators, target, key, desc) {
    var c = arguments.length, r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc, d;
    if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);
    else for (var i = decorators.length - 1; i >= 0; i--) if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
    return c > 3 && r && Object.defineProperty(target, key, r), r;
};
var __metadata = (this && this.__metadata) || function (k, v) {
    if (typeof Reflect === "object" && typeof Reflect.metadata === "function") return Reflect.metadata(k, v);
};
var __param = (this && this.__param) || function (paramIndex, decorator) {
    return function (target, key) { decorator(target, key, paramIndex); }
};
Object.defineProperty(exports, "__esModule", { value: true });
const core_1 = require("@angular/core");
let NxSystemsDropdown = class NxSystemsDropdown {
    constructor(language, config) {
        this.language = language;
        this.config = config;
    }
    trackByFn(index, item) {
        return item.id;
    }
    ngOnInit() {
        console.log('systems');
        console.log('activeSystem');
        this.systemCounter = this.systems.length;
    }
    ngOnDestroy() {
    }
};
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], NxSystemsDropdown.prototype, "activeSystem", void 0);
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], NxSystemsDropdown.prototype, "systems", void 0);
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], NxSystemsDropdown.prototype, "active", void 0);
NxSystemsDropdown = __decorate([
    core_1.Component({
        selector: 'nx-systems',
        templateUrl: './dropdowns/systems/systems.component.html',
        styleUrls: ['./dropdowns/systems/systems.component.scss']
    }),
    __param(0, core_1.Inject('languageService')),
    __param(1, core_1.Inject('configService')),
    __metadata("design:paramtypes", [Object, Object])
], NxSystemsDropdown);
exports.NxSystemsDropdown = NxSystemsDropdown;
//# sourceMappingURL=systems.component.js.map