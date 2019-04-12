import { Component, Inject, OnInit, Input, ViewEncapsulation, Renderer2, ViewChild } from '@angular/core';
import { Location }                                                                  from '@angular/common';
import { NgbModal, NgbActiveModal, NgbModalRef }                                     from '@ng-bootstrap/ng-bootstrap';
import { EmailValidator, NgForm }                                                    from '@angular/forms';
import { NxConfigService }                                                           from '../../services/nx-config';

@Component({
    selector: 'nx-modal-message-content',
    templateUrl: 'message.component.html',
    styleUrls: []
})
export class MessageModalContent {
    @Input() messageType;
    @Input() productId;
    @Input() product;
    @Input() closable;

    config: any;
    sendMessage: any;
    userName: string;
    userEmail: string;
    message: string;
    contact: boolean;
    agree: boolean;
    title: string;

    @ViewChild('feedbackForm') public feedbackForm: NgForm;

    constructor(private activeModal: NgbActiveModal,
                private configService: NxConfigService,
                private renderer: Renderer2,
                @Inject('account') private account: any,
                @Inject('process') private process: any,
                @Inject('cloudApiService') private cloudApi: any,
                @Inject('languageService') private language: any
                ) {

        this.config = configService.getConfig();
    }

    ngOnInit() {
        if (this.messageType === 'undefined') {
            this.messageType = this.language.messageType.unknown;
        }
        this.title = this.language.lang.messageType[this.messageType].replace('{{product}}', this.product);

        this.sendMessage = this.process.init(() => {
            return this.cloudApi.sendMessage(this.messageType, this.productId, this.message, this.userName, this.userEmail);
        }, {
            successMessage: this.language.lang.dialogs.messageSent
        }).then(() => {
            this.activeModal.close(true);
        });

        this.account
            .get()
            .then((account) => {
                this.userName = account.first_name + ' ' + account.last_name ;
                this.userEmail = account.email;
            });

    }

    close() {
        this.activeModal.close();
    }
}

@Component({
    selector: 'nx-modal-message',
    template: '',
    encapsulation: ViewEncapsulation.None,
    styleUrls: []
})

export class NxModalMessageComponent implements OnInit {
    config: any;
    modalRef: NgbModalRef;

    constructor(private configService: NxConfigService,
                private modalService: NgbModal) {
        this.config = configService.getConfig();
    }

    private dialog(type, product, productId) {
        // TODO: Refactor dialog to use generic dialog
        // TODO: retire loading ModalContent (CLOUD-2493)
        this.modalRef = this.modalService.open(MessageModalContent,
                {
                            windowClass: 'modal-holder',
                            backdrop: 'static'
                        });
        this.modalRef.componentInstance.closable = true;
        this.modalRef.componentInstance.messageType = type;
        this.modalRef.componentInstance.product = product;
        this.modalRef.componentInstance.productId = productId;

        return this.modalRef;
    }

    open(type, product, productId) {
        if (productId === 'undefined') {
            productId = '';
        }
        if (product === 'undefined') {
            product = '';
        }

        return this.dialog(type, product, productId).result;
    }

    ngOnInit() {
    }
}
