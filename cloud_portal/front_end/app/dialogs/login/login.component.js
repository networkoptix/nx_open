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
const common_1 = require("@angular/common");
const ng_bootstrap_1 = require("@ng-bootstrap/ng-bootstrap");
let NxModalLoginComponent = class NxModalLoginComponent {
    constructor(language, account, process, 
    // @Inject('CONFIG') private CONFIG: any,
    location, modalService) {
        this.language = language;
        this.account = account;
        this.process = process;
        this.location = location;
        this.modalService = modalService;
        this.auth = {
            email: 'ttsolov@networkoptix.com',
            password: '',
            remember: true
        };
    }
    open(content) {
        this.modalRef = this.modalService.open(content);
    }
    close() {
        this.modalRef.close();
    }
    ngOnInit() {
        this.login = this.process.init(() => {
            return this.account.login(this.auth.email, this.auth.password, this.auth.remember);
        }, {
            ignoreUnauthorized: true,
            errorCodes: {
                accountNotActivated: function () {
                    this.location.go('/activate');
                    return false;
                },
                notFound: this.language.lang.errorCodes.emailNotFound,
                portalError: this.language.lang.errorCodes.brokenAccount
            }
        }).then(() => {
            // TODO: soon
            // if (dialogSettings.params.redirect) {
            //     $location.path($routeParams.next ? $routeParams.next : Config.redirectAuthorised);
            // }
            setTimeout(function () {
                document.location.reload();
            });
        });
    }
};
NxModalLoginComponent = __decorate([
    core_1.Component({
        selector: 'nx-modal-login',
        templateUrl: './dialogs/login/login.component.html',
        // TODO: later
        // templateUrl: this.CONFIG.viewsDir + 'dialogs/login.html',
        encapsulation: core_1.ViewEncapsulation.None,
        styleUrls: []
    }),
    __param(0, core_1.Inject('languageService')),
    __param(1, core_1.Inject('account')),
    __param(2, core_1.Inject('process')),
    __metadata("design:paramtypes", [Object, Object, Object, common_1.Location,
        ng_bootstrap_1.NgbModal])
], NxModalLoginComponent);
exports.NxModalLoginComponent = NxModalLoginComponent;
//# sourceMappingURL=login.component.js.map