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
let LoginModalContent = class LoginModalContent {
    constructor(activeModal, account, process) {
        this.activeModal = activeModal;
        this.account = account;
        this.process = process;
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
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], LoginModalContent.prototype, "auth", void 0);
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], LoginModalContent.prototype, "language", void 0);
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], LoginModalContent.prototype, "login", void 0);
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], LoginModalContent.prototype, "cancellable", void 0);
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], LoginModalContent.prototype, "closable", void 0);
LoginModalContent = __decorate([
    core_1.Component({
        selector: 'ngbd-modal-content',
        templateUrl: './dialogs/login/login.component.html'
        // TODO: later
        // templateUrl: this.CONFIG.viewsDir + 'dialogs/login.html'
    }),
    __param(1, core_1.Inject('account')),
    __param(2, core_1.Inject('process')),
    __metadata("design:paramtypes", [ng_bootstrap_1.NgbActiveModal, Object, Object])
], LoginModalContent);
exports.LoginModalContent = LoginModalContent;
let NxModalLoginComponent = class NxModalLoginComponent {
    constructor(language, 
    // @Inject('CONFIG') private CONFIG: any,
    location, modalService) {
        this.language = language;
        this.location = location;
        this.modalService = modalService;
        this.auth = {
            email: '',
            password: '',
            remember: true
        };
    }
    open(keepPage) {
        this.modalRef = this.modalService.open(LoginModalContent);
        this.modalRef.componentInstance.auth = this.auth;
        this.modalRef.componentInstance.language = this.language;
        this.modalRef.componentInstance.login = this.login;
        this.modalRef.componentInstance.cancellable = !keepPage || false;
        this.modalRef.componentInstance.closable = true;
        return this.modalRef;
    }
    close() {
        this.modalRef.close();
    }
    ngOnInit() {
        // Initialization should be in LoginModalContent.ngOnInit()
    }
};
NxModalLoginComponent = __decorate([
    core_1.Component({
        selector: 'nx-modal-login',
        template: '',
        encapsulation: core_1.ViewEncapsulation.None,
        styleUrls: []
    }),
    __param(0, core_1.Inject('languageService')),
    __metadata("design:paramtypes", [Object, common_1.Location,
        ng_bootstrap_1.NgbModal])
], NxModalLoginComponent);
exports.NxModalLoginComponent = NxModalLoginComponent;
//# sourceMappingURL=login.component.js.map