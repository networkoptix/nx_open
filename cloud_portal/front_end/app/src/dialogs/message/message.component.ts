import { Component, Inject, OnInit, Input, ViewEncapsulation, Renderer2, ViewChild } from '@angular/core';
import { Location }                                                                  from '@angular/common';
import { NgbModal, NgbActiveModal, NgbModalRef }                                     from '@ng-bootstrap/ng-bootstrap';
import { EmailValidator, NgForm }                                                    from '@angular/forms';
import { NxConfigService }                                                           from '../../services/nx-config';
import {TranslateService} from "@ngx-translate/core";

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
    @Input() config;

    lang: any;
    placeholder: string;
    sendMessage: any;
    userName: string;
    userEmail: string;
    message: string;
    contact: boolean;
    agree: boolean;
    title: string;
    topic: string;
    topics: any;

    @ViewChild('feedbackForm') public feedbackForm: NgForm;

    constructor(private activeModal: NgbActiveModal,
                private renderer: Renderer2,
                private translation: TranslateService,
                @Inject('account') private account: any,
                @Inject('process') private process: any,
                @Inject('cloudApiService') private cloudApi: any,
                @Inject('languageService') private language: any
                ) {

        this.lang = this.translation.translations[this.translation.currentLang];
        this.placeholder = '';
        this.topic = '';
    }

    ngOnInit() {
        this.initForm();
        this.sendMessage = this.process.init(() => {
            return this.cloudApi.sendMessage(this.topic, this.productId, this.message, this.userName, this.userEmail, this.contact);
        }, {
            successMessage: this.language.lang.dialogs.messageSent
        }).then(() => {
            this.activeModal.close(true);
        });
    }

    close() {
        this.activeModal.close();
    }

    initForm() {
        switch (this.messageType) {
            case this.config.messageType.ipvd_page :
                this.placeholder = this.lang.messageDialogPlaceholders.feedback;
                break;
            default :
                this.placeholder = '';
        }

        this.title = this.language.lang.messageDialog.title[this.messageType].replace('{{product}}', this.product);
        this.topics = this.config.messageTopics[this.messageType].map((topic) => {
            return {
                id: topic,
                name: this.language.lang.messageDialog.topic[topic].replace('{{product}}', this.product)
            };
        });

        this.setTopic(this.topics[0]);

        this.account
            .get()
            .then((account) => {
                this.userName = account.first_name + ' ' + account.last_name ;
                this.userEmail = account.email;
            });
    }

    setTopic(topic) {
        this.topic = topic.id;
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
        this.modalRef.componentInstance.config = this.config;


        return this.modalRef;
    }

    open(type, product?, productId?) {
        if (productId === undefined) {
            productId = '';
        }
        if (product === undefined) {
            product = '';
        }

        return this.dialog(type, product, productId).result;
    }

    ngOnInit() {
    }
}
