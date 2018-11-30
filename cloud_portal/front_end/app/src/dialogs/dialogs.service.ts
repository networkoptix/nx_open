import { Inject, Injectable } from '@angular/core';

import './../dialogs/dialogs.scss';

import { NxModalLoginComponent }      from './../dialogs/login/login.component';
import { NxModalGenericComponent }    from './../dialogs/generic/generic.component';
import { NxModalShareComponent }      from './../dialogs/share/share.component';
import { NxModalDisconnectComponent } from './../dialogs/disconnect/disconnect.component';
import { NxModalRenameComponent }     from './../dialogs/rename/rename.component';
import { NxModalMergeComponent }      from './../dialogs/merge/merge.component';
import { NxModalEmbedComponent }      from './../dialogs/embed/embed.component';

@Injectable()
export class NxDialogsService {

    constructor(@Inject('ngToast') private toast: any,
                private loginModal: NxModalLoginComponent,
                private genericModal: NxModalGenericComponent,
                private disconnectModal: NxModalDisconnectComponent,
                private renameModal: NxModalRenameComponent,
                private mergeModal: NxModalMergeComponent,
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
            className: type,
            content: message,
            dismissOnTimeout: !hold,
            dismissOnClick: !hold,
            dismissButton: hold
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

    embed(system) {
        return this.embedModal.open(system);
    }
}
