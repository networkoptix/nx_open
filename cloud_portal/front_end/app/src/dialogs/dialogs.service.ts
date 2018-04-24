import { Inject, Injectable } from '@angular/core';

import { NxModalLoginComponent }      from "./../dialogs/login/login.component";
import { NxModalGeneralComponent }    from "./../dialogs/general/general.component";
import { NxModalShareComponent }      from "./../dialogs/share/share.component";
import { NxModalDisconnectComponent } from "./../dialogs/disconnect/disconnect.component";
import { NxModalRenameComponent }     from "./../dialogs/rename/rename.component";

@Injectable()
export class nxDialogsService {

    constructor(@Inject('ngToast') private toast: any,
                private loginModal: NxModalLoginComponent,
                private generalModal: NxModalGeneralComponent,
                private disconnectModal: NxModalDisconnectComponent,
                private renameModal: NxModalRenameComponent,
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

    confirm(message, title, actionLabel, actionType, cancelLabel) {
        //title, template, url, content, hasFooter, cancellable, params, closable, actionLabel, buttonType, size
        return this.generalModal.openConfirm(message, title, actionLabel, actionType, cancelLabel);
    }


    login(keepPage?) {
        return this.loginModal.open(keepPage);
    }

    share(system, user) {
        return this.shareModal.open(system, user);
    }

    disconnect(systemId) {
        return this.disconnectModal.open(systemId);
    }

    rename(systemId, systemName) {
        return this.renameModal.open(systemId, systemName);
    }
}
