import {
    Component,
    Inject,
    OnInit,
    Input,
    ViewEncapsulation,
    ViewChild,
    Renderer2,
    AfterViewInit
} from '@angular/core';
import { Location, LocationStrategy, PathLocationStrategy } from '@angular/common';
import { NgbModal, NgbActiveModal, NgbModalRef }            from '@ng-bootstrap/ng-bootstrap';
import { NxModalGenericComponent }                          from "../generic/generic.component";

@Component({
    selector: 'ngbd-modal-content',
    templateUrl: 'login.component.html',
    styleUrls: [],
    providers: [Location, {provide: LocationStrategy, useClass: PathLocationStrategy}],
})
export class LoginModalContent implements OnInit, AfterViewInit{
    @Input() auth;
    @Input() language;
    @Input() login;
    @Input() cancellable;
    @Input() closable;
    @Input() location;
    @Input() keepPage;

    nx_wrong_password: boolean;
    nx_account_blocked: boolean;

    @ViewChild('loginForm') loginForm: HTMLFormElement;

    constructor(public activeModal: NgbActiveModal,
                @Inject('configService') private configService: any,
                @Inject('account') private account: any,
                @Inject('process') private process: any,
                @Inject('cloudApiService') private cloudApi: any,
                private renderer: Renderer2,
                private genericModal: NxModalGenericComponent) {

        this.nx_wrong_password = false;
    }

    resendActivation(email) {
        this.activeModal.close();

        this.process.init(() => {
                return this.cloudApi.reactivate(email);
            }, {
                errorCodes: {
                    forbidden: this.language.lang.errorCodes.accountAlreadyActivated,
                    notFound: this.language.lang.errorCodes.emailNotFound
                },
                holdAlerts: true,
                errorPrefix: this.language.lang.errorCodes.cantSendConfirmationPrefix
            })
            .run()
            .then(() => {
                this.genericModal.openConfirm(
                    'Check your inbox and visit provided link to activate account',
                    'Activation email sent',
                    'OK');
            });
    }

    gotoRegister() {
        // TODO: Repace this once 'register' page is moved to A5
        // AJS and A5 routers freak out about route change *****
        //this.location.go('/register');
        document.location.href = '/register';
        this.activeModal.close();
    }

    resetForm() {
        if (!this.loginForm.valid) {
            this.loginForm.controls['email'].setErrors(null);
            this.loginForm.controls['password'].setErrors(null);
            this.nx_wrong_password = false;
            this.nx_account_blocked = false;
        }
    }

    ngOnInit() {
        this.auth.password = '';

        this.login = this.process.init(() => {
            this.loginForm.controls['email'].setErrors(null);
            this.loginForm.controls['password'].setErrors(null);
            this.nx_wrong_password = false;
            this.nx_account_blocked = false;

            return this.account.login(this.auth.email, this.auth.password, this.auth.remember);
        }, {
            ignoreUnauthorized: true,
            errorCodes: {
                accountNotActivated: () => {
                    this.auth.password = '';
                    this.loginForm.controls['password'].markAsPristine();
                    this.loginForm.controls['password'].markAsUntouched();

                    this.loginForm.controls['email'].setErrors({'not_activated': true});
                    this.renderer.selectRootElement('#email').select();
                },
                notAuthorized: () => {
                    this.nx_wrong_password = true;
                    this.loginForm.controls['password'].setErrors({'nx_wrong_password': true});
                    this.auth.password = '';

                    this.renderer.selectRootElement('#password').focus();

                },
                notFound: () => {
                    this.auth.password = '';
                    this.loginForm.controls['password'].markAsPristine();
                    this.loginForm.controls['password'].markAsUntouched();

                    this.loginForm.controls['email'].setErrors({'no_user': true});
                    this.renderer.selectRootElement('#email').select();
                },
                accountBlocked: () => {
                    this.loginForm.controls['password'].markAsPristine();
                    this.loginForm.controls['password'].markAsUntouched();

                    this.nx_account_blocked = true;
                    this.loginForm.controls['password'].setErrors({'nx_account_blocked': true});
                },
                portalError: this.language.lang.errorCodes.brokenAccount
            }
        }).then(() => {
            if (this.keepPage) {
                document.location.href = (this.location.path() !== '') ? this.location.path() : this.configService.config.redirectAuthorised;
            } else {
                setTimeout(() => {
                    document.location.href = this.configService.config.redirectAuthorised;
                });
            }
        });
    }

    ngAfterViewInit() {
        this.renderer.selectRootElement('#email').focus();
    }
}

@Component({
    selector: 'nx-modal-login',
    template: '',
    encapsulation: ViewEncapsulation.None,
    styleUrls: []
})
export class NxModalLoginComponent implements OnInit {
    login: any;
    modalRef: NgbModalRef;
    auth = {
        email: '',
        password: '',
        remember: true
    };
    location: Location;

    constructor(@Inject('languageService') private language: any,
                private modalService: NgbModal,
                location: Location) {

        this.location = location;
    }

    private dialog(keepPage?) {
        this.modalRef = this.modalService.open(LoginModalContent, {size: 'sm', centered: true});
        this.modalRef.componentInstance.auth = this.auth;
        this.modalRef.componentInstance.language = this.language;
        this.modalRef.componentInstance.login = this.login;
        this.modalRef.componentInstance.cancellable = !keepPage || false;
        this.modalRef.componentInstance.closable = true;
        this.modalRef.componentInstance.location = this.location;
        this.modalRef.componentInstance.keepPage = keepPage || true;

        return this.modalRef;
    }

    open(keepPage?) {
        return this.dialog(keepPage).result;
    }

    close() {
        this.modalRef.close();
    }

    ngOnInit() {
        // Initialization should be in LoginModalContent.ngOnInit()
    }
}
