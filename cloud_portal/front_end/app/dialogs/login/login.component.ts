import { Component, Inject, OnInit, ViewEncapsulation } from '@angular/core';
import { Location } from '@angular/common';
import { NgbModal, ModalDismissReasons } from '@ng-bootstrap/ng-bootstrap';
// import { NxProcessButtonComponent } from "../../components/process-button/process-button.component";

@Component({
    selector: 'nx-modal-login',
    templateUrl: './dialogs/login/login.component.html',
    encapsulation: ViewEncapsulation.None,
    styleUrls: []
})

export class NxModalLoginComponent implements OnInit {
    closeResult: string;
    auth = {
        email: 'tsanko.tsolov@gmail.com',
        password: '',
        remember: true
    };

    constructor(@Inject('languageService') private language: any,
                @Inject('accountService') private account: any,
                @Inject('processService') private process: any,
                private location: Location,
                private modalService: NgbModal) {
    }

    open(content) {
        console.log('LANG', this.language);
        const modalRef = this.modalService.open(content);

        modalRef.result.then((result) => {
            this.closeResult = `Closed with: ${result}`;
        }, (reason) => {
            this.closeResult = `Dismissed ${this.getDismissReason(reason)}`;
        });

    }

    login () {
        debugger;
        console.log('LANG', this.language);
        // alert('LOGIN!');
        this.process.init(function () {
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
        }).then(function () {
            // if (dialogSettings.params.redirect) {
            //     $location.path($routeParams.next ? $routeParams.next : Config.redirectAuthorised);
            // }
            // setTimeout(function () {
            //     document.location.reload();
            // });
        });
    }

    private getDismissReason(reason: any): string {
        if (reason === ModalDismissReasons.ESC) {
            return 'by pressing ESC';
        } else if (reason === ModalDismissReasons.BACKDROP_CLICK) {
            return 'by clicking on a backdrop';
        } else {
            return `with: ${reason}`;
        }
    }

    ngOnInit() {

    }

}
