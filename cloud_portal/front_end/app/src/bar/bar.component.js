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
const index_1 = require("../core/index");
const login_component_1 = require("../../src/dialogs/login/login.component");
let BarComponent = class BarComponent {
    constructor(uuid2, language, cloudApi, http, quoteService, loginModal, changeDetector) {
        this.uuid2 = uuid2;
        this.language = language;
        this.cloudApi = cloudApi;
        this.http = http;
        this.quoteService = quoteService;
        this.loginModal = loginModal;
        this.changeDetector = changeDetector;
        this.title = 'bar';
        // this.uuid2 = uuid2;
        //this.language = language;
        //this.cloudApi = cloudApi;
    }
    // getLanguages(): any {
    //     return this.http.get('/static/languages.json', {});
    // }
    login() {
        // this.loginModal.open();
    }
    ngOnInit() {
        this.serviceMessage = this.uuid2.newguid();
        this.quoteService
            .getRandomQuote({ category: 'dev' })
            .subscribe((data) => {
            this.message = data.value;
            // this.changeDetector.detectChanges();
        }, (err) => {
            console.log(err.error);
            console.log(err.name);
            console.log(err.message);
            console.log(err.status);
        });
        // this.getLanguages()
        //         .subscribe((data: any) => {
        //             console.log(data);
        //             this.activeLanguage = data;
        //         });
        this.cloudApi
            .getLanguages()
            .then((data) => {
            // console.log('Data: ', data.data);
            // console.log('Length: ', data.data.length);
        });
    }
};
BarComponent = __decorate([
    core_1.Component({
        selector: 'bar-component',
        templateUrl: 'bar.component.html'
    }),
    __param(0, core_1.Inject('uuid2Service')),
    __param(1, core_1.Inject('languageService')),
    __param(2, core_1.Inject('cloudApiService')),
    __metadata("design:paramtypes", [Object, Object, Object, http_1.HttpClient,
        index_1.QuoteService,
        login_component_1.NxModalLoginComponent,
        core_1.ChangeDetectorRef])
], BarComponent);
exports.BarComponent = BarComponent;
//# sourceMappingURL=bar.component.js.map