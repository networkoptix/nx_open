import {Component, Inject, OnInit, ViewEncapsulation} from '@angular/core';
import {Location} from '@angular/common';
import {NgbModal, NgbModalRef} from '@ng-bootstrap/ng-bootstrap';
import {EmailValidator} from '@angular/forms';

@Component({
    selector: 'nx-modal-login',
    templateUrl: './dialogs/login/login.component.html',
    // TODO: later
    // templateUrl: this.CONFIG.viewsDir + 'dialogs/login.html',
    encapsulation: ViewEncapsulation.None,
    styleUrls: []
})

export class NxModalLoginComponent implements OnInit {
    login: any;
    modalRef: NgbModalRef;
    closeResult: string;
    auth = {
        email: 'ttsolov@networkoptix.com',
        password: '',
        remember: true
    };

    constructor(@Inject('languageService') private language: any,
                @Inject('account') private account: any,
                @Inject('process') private process: any,
                // @Inject('CONFIG') private CONFIG: any,
                private location: Location,
                private modalService: NgbModal) {
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
}
