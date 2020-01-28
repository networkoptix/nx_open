import {
    Component, Inject, Input, OnInit,
    Renderer2, ViewChild
} from '@angular/core';
import { NgbActiveModal }            from '@ng-bootstrap/ng-bootstrap';
import { EmailValidator, NgForm }    from '@angular/forms';
import { NxConfigService }           from '../../services/nx-config';
import { WINDOW }                    from '../../services/window-provider';
import { NxLanguageProviderService } from '../../services/nx-language-provider';
import { NxProcessService }          from '../../services/process.service';
import { NxCloudApiService }         from '../../services/nx-cloud-api';


export interface MessageParams {
    disclaimer: string;
    email?: string;
    asset: string;
    assetId?: string;
    to?: string;
}

interface Subject {
    id: string;
    name: string;
}

@Component({
    selector: 'nx-modal-message-content',
    templateUrl: 'message.component.html',
    styleUrls: []
})
export class MessageModalContent implements OnInit {
    @Input() account;
    @Input() messageType;
    @Input() data;
    @Input() closable;

    LANG: any;

    config: any;
    placeholder: string;
    sendMessage: any;
    userName: string;
    userEmail: string;
    message: string;
    agree: boolean;
    title: string;
    subject: string;
    subjectMessage: string;
    subjects: Subject[];
    url: string;

    @ViewChild('feedbackForm', { static: true }) public feedbackForm: NgForm;

    constructor(private activeModal: NgbActiveModal,
                private renderer: Renderer2,
                private language: NxLanguageProviderService,
                private configService: NxConfigService,
                private processService: NxProcessService,
                private cloudApiService: NxCloudApiService,
                @Inject(WINDOW) private window: Window,
    ) {
        this.placeholder = '';
        this.subject = '';
        this.subjectMessage = '';
        this.url = this.window.location.href;
        this.config = configService.getConfig();
    }

    ngOnInit() {
        this.LANG = this.language.getTranslations();

        this.initForm();
        this.sendMessage = this.processService.createProcess(() => {
            const asset = this.data.assetId || this.data.asset;

            return this.cloudApiService.sendMessage(this.subject, asset, this.message, this.userName, this.userEmail).toPromise();
        }, {
            successMessage: this.LANG.dialogs.message.sent
        }).then(() => {
            this.activeModal.close(true);
        });
    }

    close() {
        this.activeModal.close();
    }

    initForm() {
        this.placeholder = '';
        if (this.messageType === this.config.messageType.ipvd_page) {
            this.placeholder = this.LANG.messageDialogPlaceholders.feedback;
        }

        const title = this.LANG.dialogs.message.title[this.messageType];
        if (this.messageType !== this.config.messageType.integration) {
            this.title = title.replace('{{asset}}', this.data.asset);
        } else {
            this.title = title.replace('{{companyName}}', this.data.to);
        }
        this.subjects = this.config.messageSubjects[this.messageType].map((subject) => {
            return {
                value: subject,
                name: this.LANG.dialogs.message.subject[subject].replace('{{asset}}', this.data.asset)
            };
        });

        this.setSubject(this.subjects[0]);

        this.account
            .get()
            .then((account) => {
                if (account) {
                    this.userName = `${account.first_name} ${account.last_name}`;
                    this.userEmail = account.email;
                }
            });
    }

    setSubject(subject: any) {
        this.subject = subject.value;
        this.subjectMessage = subject.name;
    }
}
