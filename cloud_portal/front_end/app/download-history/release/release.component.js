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
let ReleaseComponent = class ReleaseComponent {
    constructor(language, configService) {
        this.language = language;
        this.configService = configService;
    }
    ngOnInit() {
    }
};
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], ReleaseComponent.prototype, "release", void 0);
ReleaseComponent = __decorate([
    core_1.Component({
        selector: 'nx-release',
        templateUrl: './download-history/release/release.component.html',
    }),
    __param(0, core_1.Inject('languageService')),
    __param(1, core_1.Inject('configService')),
    __metadata("design:paramtypes", [Object, Object])
], ReleaseComponent);
exports.ReleaseComponent = ReleaseComponent;
//# sourceMappingURL=release.component.js.map