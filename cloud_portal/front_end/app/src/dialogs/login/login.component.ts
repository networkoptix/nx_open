import { Component, Inject, OnInit, Input, ViewEncapsulation, ViewChild, ElementRef } from '@angular/core';
import { Location, LocationStrategy, PathLocationStrategy }                           from '@angular/common';
import { NgbModal, NgbActiveModal, NgbModalRef }                                      from '@ng-bootstrap/ng-bootstrap';

@Component({
    selector: 'ngbd-modal-content',
    templateUrl: 'login.component.html',
    styleUrls: [],
    providers: [Location, {provide: LocationStrategy, useClass: PathLocationStrategy}],
})
export class LoginModalContent {
    @Input() auth;
    @Input() language;
    @Input() login;
    @Input() cancellable;
    @Input() closable;
    @Input() location;

    @ViewChild('loginForm') loginForm: HTMLFormElement;
    @ViewChild('email') email: ElementRef;

    userStatus: {
        notValidUsername: boolean,
        notValidPassword: boolean,
    };

    constructor(public activeModal: NgbActiveModal,
                @Inject('account') private account: any,
                @Inject('process') private process: any) {

        this.userStatus = {
            notValidUsername: false,
            notValidPassword: false,
        }
    }

    gotoRegister() {
        // TODO: Repace this once 'register' page is moved to A5
        // AJS and A5 routers freak out about route change *****
        //this.location.go('/register');
        document.location.href = '/register';
        this.activeModal.close();
    }

    ngOnInit() {
        this.login = this.process.init(() => {
            return this.account.login(this.auth.email, this.auth.password, this.auth.remember);
        }, {
            ignoreUnauthorized: true,
            errorCodes: {
                accountNotActivated: () => {
                    // TODO: Repace this once 'activate' page is moved to A5
                    // AJS and A5 routers freak out about route change *****
                    //this.location.go('/activate');
                    document.location.href = '/activate';
                    // *****************************************************
                    return false;
                },
                notAuthorized: () => {
                    alert(this.language.lang.errorCodes.notAuthorized);
                },
                notFound: () => {
                    // alert(this.language.lang.errorCodes.emailNotFound)
                    this.auth.password = '';
                    this.loginForm.controls['email'].setErrors({'no_user': true});
                    this.loginForm.controls['password'].markAsPristine();
                    this.loginForm.controls['password'].markAsUntouched();

                    <HTMLInputElement>this.email.nativeElement.select();

                    // Using non-standard form validation is because
                    // it breaks select() functionality -> switches ElementRef to FormControl
                    this.userStatus.notValidUsername = true;
                },
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

    open(keepPage?) {
        this.modalRef = this.modalService.open(LoginModalContent, {size: 'sm', centered: true});
        this.modalRef.componentInstance.auth = this.auth;
        this.modalRef.componentInstance.language = this.language;
        this.modalRef.componentInstance.login = this.login;
        this.modalRef.componentInstance.cancellable = !keepPage || false;
        this.modalRef.componentInstance.closable = true;
        this.modalRef.componentInstance.location = this.location;

        return this.modalRef;
    }

    close() {
        this.modalRef.close();
    }

    ngOnInit() {
        // Initialization should be in LoginModalContent.ngOnInit()
    }
}
