import { Inject, Injectable } from '@angular/core';
import { DOCUMENT, Location } from '@angular/common';
import { DomSanitizer }       from '@angular/platform-browser';
import { NgbModal }           from '@ng-bootstrap/ng-bootstrap';
import { Router }             from '@angular/router';

import './../dialogs/dialogs.scss';

import { NxToastService }            from './toast.service';
import { LoginModalContent }         from './login/login.component';
import { NxLanguageProviderService } from '../services/nx-language-provider';
import { GenericModalContent }       from './generic/generic.component';
import { AddUserModalContent }       from './add-user/add-user.component';
import { DisconnectModalContent }    from './disconnect/disconnect.component';
import { RenameModalContent }        from './rename/rename.component';
import { MessageModalContent }       from './message/message.component';
import { EmbedModalContent }         from './embed/embed.component';
import { MergeModalContent }         from './merge/merge.component';
import { NxConfigService }           from '../services/nx-config';
import { NxAccountService }          from '../services/account.service';
import { ApplyModalContent }         from './apply/apply.component';
import { RemoveUserModalContent }    from './remove-user/remove-user.component';

@Injectable({ providedIn: 'root' })
export class NxDialogsService {

    LANG: any = {};
    CONFIG: any = {};
    location: any;
    closeResult: any;

    constructor(@Inject(DOCUMENT) private document: any,
                private modalService: NgbModal,
                private toastService: NxToastService,
                private language: NxLanguageProviderService,
                private domSanitizer: DomSanitizer,
                location: Location,
                private configService: NxConfigService,
                private router: Router
    ) {
        this.LANG = this.language.getTranslations();
        this.CONFIG = this.configService.getConfig();
        this.location = location;
    }

    dismiss() {
        this.toastService.remove();
    }

    notify(message, type, hold?) {
        type = type || 'info';
        hold = hold || false;

        const options = {
            autohide: !hold,
            classname: type,
            delay: this.CONFIG.alertTimeout
        };

        return this.toastService.show(message, options);
    }

    createModal(modal, options, inputs) {
        const modalRef = this.modalService.open(modal, options);
        Object.assign(modalRef.componentInstance, inputs);
        return modalRef.result;
    }

    alert(message, title, footerClass?) {
        const options: any = {
            windowClass: 'modal-holder',
            backdrop   : 'static'
        };

        const params: any = {
            message    : this.domSanitizer.bypassSecurityTrustHtml(message),
            title,
            actionLabel: this.LANG.dialogs.okButton,
            buttonType : 'default',
            cancelLabel: this.LANG.dialogs.cancelButton,
            buttonClass: 'btn-primary',
            footerClass: footerClass || '',
            hasFooter  : true,
            cancellable: true,
            closable   : true,
        };

        return this.createModal(GenericModalContent, options, params);
    }

    apply(applyFunc, discardFunc, form) {
        // Blur activeElement to prevent ExpressionChangedAfterItHasBeenCheckedError
        if (document.activeElement instanceof HTMLElement) {
            document.activeElement.blur();
        }
        return this.createModal(ApplyModalContent, {}, {applyFunc, discardFunc, form});
    }

    confirm(message, title, actionLabel, actionType?, cancelLabel?, footerClass?) {
        const options: any = {
            windowClass: 'modal-holder',
            backdrop   : 'static'
        };

        const params: any = {
            message : message ? this.domSanitizer.bypassSecurityTrustHtml(message) : '',
            title,
            actionLabel,
            buttonType : actionType || 'default',
            cancelLabel,
            buttonClass : actionType || 'btn-primary',
            footerClass: footerClass || '',
            hasFooter : true,
            cancellable : false,
            closable : true,
        };

        return this.createModal(GenericModalContent, options, params);
    }

    login(account: NxAccountService, keepPage?, redirectClose?) {
        const options: any = {
            windowClass: 'modal-holder',
            backdrop   : 'static',
            size       : 'sm'
        };

        const params: any = {
            account,
            login        : this.login,
            cancellable  : !keepPage || false,
            closable     : true,
            location     : this.location,
            keepPage     : (keepPage !== undefined) ? keepPage : true,
            redirectClose: redirectClose || false
        };

        return this.createModal(LoginModalContent, options, params)
            // handle how the dialog was closed
            // required if we need to have dismissible dialog otherwise
            // will raise a JS error ( Uncaught [in promise] )
            .then((result) => {
                this.closeResult = `Closed with: ${result}`;

                if (redirectClose && result === 'canceled') {
                    return this.router.navigate([this.CONFIG.redirectUnauthorised]);
                }
            }, (reason) => {
                this.closeResult = 'Dismissed';
            });
    }

    addUser(account, system, user?) {
        const options: any = {
            windowClass: 'modal-holder',
            backdrop   : 'static'
        };

        const params: any = {
            account,
            system,
            user,
            closable   : true,
        };

        return this.createModal(AddUserModalContent, options, params);
    }

    disconnect(systemId) {
        const options: any = {
            windowClass: 'modal-holder',
            backdrop   : 'static'
        };

        const params: any = {
            systemId,
            closable: true,
        };

        return this.createModal(DisconnectModalContent, options, params);
    }

    removeUser(system, user) {
        const options: any = {
            windowClass: 'modal-holder',
            backdrop   : 'static'
        };

        const params: any = {
            system,
            user,
            closable   : true,
        };

        return this.createModal(RemoveUserModalContent, options, params);
    }

    rename(systemId, systemName) {
        const options: any = {
            windowClass: 'modal-holder',
            backdrop   : 'static'
        };

        const params: any = {
            systemId,
            systemName,
            closable: true,
        };

        return this.createModal(RenameModalContent, options, params);
    }

    merge(system, systems, user) {
        const options: any = {
            windowClass: 'modal-holder',
            backdrop   : 'static'
        };

        const params: any = {
            user,
            system,
            systems,
            closable   : true,
        };

        return this.createModal(MergeModalContent, options, params);
    }

    message(account: NxAccountService, type, data): Promise<any> {
        const options: any = {
            windowClass: 'modal-holder',
            backdrop   : 'static'
        };

        const params: any = {
            account,
            messageType : type,
            data,
            closable: true,
        };

        return this.createModal(MessageModalContent, options, params);
    }

    embed(systemId) {
        const options: any = {
            windowClass: 'modal-holder',
            backdrop   : 'static'
        };

        const params: any = {
            closable   : true,
        };

        return this.createModal(EmbedModalContent, options, params);
    }
}
