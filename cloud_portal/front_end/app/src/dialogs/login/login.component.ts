import { Component, Inject, OnInit, Input, ViewEncapsulation } from '@angular/core';
import { Location }                                            from '@angular/common';
import { NgbModal, NgbActiveModal, NgbModalRef }               from '@ng-bootstrap/ng-bootstrap';
import { EmailValidator }                                      from '@angular/forms';
import { TranslateService }                                    from '@ngx-translate/core';

@Component({
    selector: 'ngbd-modal-content',
    templateUrl: 'login.component.html',
    styleUrls: ['login.component.scss']
})
export class LoginModalContent {
    @Input() auth;
    @Input() language;
    @Input() login;
    @Input() cancellable;
    @Input() closable;

    constructor(public activeModal: NgbActiveModal,
                @Inject('account') private account: any,
                @Inject('process') private process: any) {
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
        email: 'ttsolov@networkoptix.com',
        password: '2l2b2l2n1ts2',
        remember: true
    };

    constructor(@Inject('languageService') private language: any,
                // @Inject('CONFIG') private CONFIG: any,
                private location: Location,
                private modalService: NgbModal) {
    }

    open(keepPage?) {
        this.modalRef = this.modalService.open(LoginModalContent, {size: 'sm'});
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
}
