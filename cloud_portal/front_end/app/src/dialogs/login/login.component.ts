import {
    Component,
    Inject,
    OnInit,
    Input,
    ViewEncapsulation,
    ViewChild,
    Renderer2
}                                                                     from '@angular/core';
import { DOCUMENT, Location, LocationStrategy, PathLocationStrategy } from '@angular/common';
import { NgbModal, NgbActiveModal, NgbModalRef }                      from '@ng-bootstrap/ng-bootstrap';
import { NxModalGenericComponent }                                    from '../generic/generic.component';

@Component({
    selector: 'ngbd-modal-content',
    templateUrl: 'login.component.html',
    styleUrls: [],
    providers: [Location, {provide: LocationStrategy, useClass: PathLocationStrategy}],
})
export class LoginModalContent implements OnInit {
    @Input() language;
    @Input() configService;
    @Input() login;
    @Input() cancellable;
    @Input() closable;
    @Input() location;
    @Input() keepPage;

    auth: any;
    password: string;
    remember: boolean;

    nx_wrong_password: boolean;
    nx_account_blocked: boolean;

    @ViewChild('loginForm') loginForm: HTMLFormElement;

    constructor(public activeModal: NgbActiveModal,
                @Inject('account') private account: any,
                @Inject('process') private process: any,
                @Inject('cloudApiService') private cloudApi: any,
                @Inject('localStorageService') private localStorage: any,
                @Inject(DOCUMENT) private document: any,
                private renderer: Renderer2,
                private genericModal: NxModalGenericComponent) {

        this.auth = this.localStorage;
        this.password = '';
        this.remember = true;
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
        this.document.location.href = '/register';
        this.activeModal.close();
    }

    resetForm() {
        if (!this.loginForm.valid) {
            this.loginForm.controls['login_password'].setErrors(null);
            this.nx_wrong_password = false;
            this.nx_account_blocked = false;
        }
    }

    ngOnInit() {
        this.password = '';

        this.login = this.process.init(() => {
            this.loginForm.controls['login_email'].setErrors(null);
            this.loginForm.controls['login_password'].setErrors(null);
            this.nx_wrong_password = false;
            this.nx_account_blocked = false;

            return this.account.login(this.auth.email, this.password, this.remember);
        }, {
            ignoreUnauthorized: true,
            errorCodes: {
                accountNotActivated: () => {
                    this.password = '';
                    this.loginForm.controls['login_password'].markAsPristine();
                    this.loginForm.controls['login_password'].markAsUntouched();

                    this.loginForm.controls['login_email'].setErrors({'not_activated': true});
                    this.renderer.selectRootElement('#login_email').select();
                },
                notAuthorized: () => {
                    this.nx_wrong_password = true;
                    this.loginForm.controls['login_password'].setErrors({'nx_wrong_password': true});
                    this.password = '';

                    this.renderer.selectRootElement('#login_password').focus();

                },
                notFound: () => {
                    this.password = '';
                    this.loginForm.controls['login_password'].markAsPristine();
                    this.loginForm.controls['login_password'].markAsUntouched();

                    this.loginForm.controls['login_email'].setErrors({'no_user': true});
                    this.renderer.selectRootElement('#login_email').select();
                },
                accountBlocked: () => {
                    this.loginForm.controls['login_password'].markAsPristine();
                    this.loginForm.controls['login_password'].markAsUntouched();

                    this.nx_account_blocked = true;
                    this.loginForm.controls['login_password'].setErrors({'nx_account_blocked': true});
                },
                wrongParameters: () => {
                },
                portalError: this.language.lang.errorCodes.brokenAccount
            }
        }).then(() => {
            if (this.keepPage) {
                if (this.location.path() !== '') {
                    this.document.location.href = this.location.path();
                } else {
                    // prevent unnecessary reload
                    if (this.document.location.pathname !== this.configService.config.redirectAuthorised) {
                        this.document.location.href = this.configService.config.redirectAuthorised;
                    }
                }
            } else {
                setTimeout(() => {
                    this.document.location.href = this.configService.config.redirectAuthorised;
                });
            }
        });
    }

    close(redirect?) {
        // prevent unnecessary reload
        if (redirect && this.document.location.pathname !== this.configService.config.redirectUnauthorised) {
            this.document.location.href = this.configService.config.redirectUnauthorised;
        }

        this.activeModal.close();
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
    location: Location;
    closeResult: string;

    constructor(@Inject('languageService') private language: any,
                @Inject('configService') private configService: any,
                private modalService: NgbModal,
                location: Location) {

        this.location = location;
    }

    private dialog(keepPage?) {
        this.modalRef = this.modalService.open(LoginModalContent, {size: 'sm', centered: true});
        this.modalRef.componentInstance.language = this.language;
        this.modalRef.componentInstance.configService = this.configService;
        this.modalRef.componentInstance.login = this.login;
        this.modalRef.componentInstance.cancellable = !keepPage || false;
        this.modalRef.componentInstance.closable = true;
        this.modalRef.componentInstance.location = this.location;
        this.modalRef.componentInstance.keepPage = (keepPage !== undefined) ? keepPage : true;

        return this.modalRef;
    }

    open(keepPage?) {
        return this.dialog(keepPage)
                   .result
                   // handle how the dialog was closed
                   // required if we need to have dismissible dialog otherwise
                   // will raise a JS error ( Uncaught [in promise] )
                   .then((result) => {
                       this.closeResult = `Closed with: ${result}`;
                   }, (reason) => {
                       this.closeResult = `Dismissed`;
                   });
    }

    ngOnInit() {
        // Initialization should be in LoginModalContent.ngOnInit()
    }
}
