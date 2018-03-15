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
const http_1 = require("@angular/common/http");
const ng_bootstrap_1 = require("@ng-bootstrap/ng-bootstrap");
let NxLanguageDropdown = class NxLanguageDropdown {
    constructor(cloudApi, language, httpClient, dropdown, changeDetector) {
        this.cloudApi = cloudApi;
        this.language = language;
        this.httpClient = httpClient;
        this.dropdown = dropdown;
        this.changeDetector = changeDetector;
        this.activeLanguage = {
            language: '',
            name: ''
        };
        this.languages = [];
    }
    changeLanguage(lang) {
        this.activeLanguage = lang;
    }
    ngOnInit() {
        this.accountMode = this.accountMode || false;
        this.cloudApi
            .getLanguages()
            .then((data) => {
            this.languages = data.data;
            this.activeLanguage = this.languages.find(lang => {
                return (lang.language === this.language.lang.language);
            });
            if (!this.activeLanguage) {
                this.activeLanguage = this.languages[0];
            }
        });
    }
};
NxLanguageDropdown = __decorate([
    core_1.Component({
        selector: 'nx-language-select',
        templateUrl: './dropdown/language.component.html',
        styleUrls: ['./dropdown/language.component.scss'],
        inputs: ['accountMode'],
    }),
    __param(0, core_1.Inject('cloudApiService')),
    __param(1, core_1.Inject('languageService')),
    __metadata("design:paramtypes", [Object, Object, http_1.HttpClient,
        ng_bootstrap_1.NgbDropdownModule,
        core_1.ChangeDetectorRef])
], NxLanguageDropdown);
exports.NxLanguageDropdown = NxLanguageDropdown;
//# sourceMappingURL=language.component.js.map