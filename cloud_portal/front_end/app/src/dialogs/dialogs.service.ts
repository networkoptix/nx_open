import { Inject, Injectable }         from '@angular/core';
import { NxModalLoginComponent }      from './login/login.component';
import { NxModalGenericComponent }    from './generic/generic.component';
import { NxModalShareComponent }      from './share/share.component';
import { NxModalDisconnectComponent } from './disconnect/disconnect.component';
import { NxModalRenameComponent }     from './rename/rename.component';
import { NxModalMergeComponent }      from './merge/merge.component';
import { NxModalEmbedComponent }      from './embed/embed.component';
import { NxModalMessageComponent }    from './message/message.component';

@Injectable()
export class NxDialogsService {

    constructor(@Inject('ngToast') private toast: any,
                private loginModal: NxModalLoginComponent,
                private genericModal: NxModalGenericComponent,
                private disconnectModal: NxModalDisconnectComponent,
                private renameModal: NxModalRenameComponent,
                private mergeModal: NxModalMergeComponent,
                private messageModel: NxModalMessageComponent,
                private embedModal: NxModalEmbedComponent,
                private shareModal: NxModalShareComponent) {

    }

    dismiss() {
        this.toast.dismiss();
    }

    notify(message, type, hold?) {
        type = type || 'info';
        hold = hold || false;

        return this.toast.create({
            className       : type,
            content         : message,
            dismissOnTimeout: !hold,
            dismissOnClick  : !hold,
            dismissButton   : hold
        });
    }

    confirm(message, title, actionLabel, actionType?, cancelLabel?) {
        // title, template, url, content, hasFooter, cancellable, params, closable, actionLabel, buttonType, size
        return this.genericModal.openConfirm(message, title, actionLabel, actionType, cancelLabel);
    }


    login(keepPage?) {
        return this.loginModal.open(keepPage);
    }

    share(system?, user?) {
        return this.shareModal.open(system, user);
    }

    disconnect(systemId) {
        return this.disconnectModal.open(systemId);
    }

    rename(systemId, systemName) {
        return this.renameModal.open(systemId, systemName);
    }

    merge(system) {
        return this.mergeModal.open(system);
    }

    message(type, product, productId) {
        return this.messageModel.open(type, product, productId);
    }

    embed(system) {
        return this.embedModal.open(system);
    }
}
