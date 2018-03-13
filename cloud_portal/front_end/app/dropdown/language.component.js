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
Object.defineProperty(exports, "__esModule", { value: true });
const core_1 = require("@angular/core");
const http_1 = require("@angular/common/http");
const ng_bootstrap_1 = require("@ng-bootstrap/ng-bootstrap");
const cloud_api_1 = require("../scripts/services/cloud_api");
let NxLanguageDropdown = class NxLanguageDropdown {
    // function (language) {
    //     if (!this.accountMode) {
    //         if (language == L.language) {
    //             return;
    //         }
    //         this.api.get().changeLanguage(language).then(function () {
    //             window.location.reload();
    //         });
    //     } else {
    //         this.ngModel = language;
    //         this.activeLanguage = _.find(this.languages, function (lang) {
    //             return lang.language == this.ngModel;
    //         });
    //     }
    // }
    constructor(httpClient, api, dropdown, changeDetector) {
        this.httpClient = httpClient;
        this.api = api;
        this.dropdown = dropdown;
        this.changeDetector = changeDetector;
        this.activeLanguage = {
            language: '',
            name: ''
        };
    }
    getLanguages() {
        this.httpClient.get('/static/languages.json');
    }
    ngOnInit() {
        // this.getLanguages()
        //         .subscribe((data: any) => {
        //             this.activeLanguage = data.data;
        //         });
        console.log(this.activeLanguage);
        // this.api.get().getLanguages()
        //         .subscribe((data: any) => {
        //             let languages = data.data;
        //
        //             console.log(data);
        //
        //             this.activeLanguage = languages.find(lang => {
        //                 return (lang.language === languageService.get().lang.language);
        //             });
        //
        //             if (!this.activeLanguage) {
        //                 this.activeLanguage = languages[0];
        //             }
        //             this.languages = languages;
        //         });
        // this.changeLanguage();
    }
};
NxLanguageDropdown = __decorate([
    core_1.Component({
        selector: 'nx-language-select',
        templateUrl: './dropdown/language.component.html',
        styleUrls: ['./dropdown/language.component.scss']
    }),
    __metadata("design:paramtypes", [http_1.HttpClient,
        cloud_api_1.cloudApiService,
        ng_bootstrap_1.NgbDropdownModule,
        core_1.ChangeDetectorRef])
], NxLanguageDropdown);
exports.NxLanguageDropdown = NxLanguageDropdown;
//# sourceMappingURL=language.component.js.map