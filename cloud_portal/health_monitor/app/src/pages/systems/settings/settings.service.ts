import { Injectable, OnDestroy }       from '@angular/core';
import { BehaviorSubject, Observable } from 'rxjs';
import { NxCloudApiService }           from '../../../services/nx-cloud-api';
import { NxConfigService }             from '../../../services/nx-config';
import { NxDialogsService }            from '../../../dialogs/dialogs.service';
import { NxAccountService }            from '../../../services/account.service';
import { NxUriService }                from '../../../services/uri.service';
import { NxMenuService }               from '../../../components/menu/menu.service';

@Injectable({
    providedIn: 'root'
})
export class NxSettingsService implements OnDestroy {
    config: any = {};
    footerSubject = new BehaviorSubject(false);
    systemSubject = new BehaviorSubject(undefined);
    selectedSectionSubject = new BehaviorSubject([]);

    private _mergeTarget = '';
    share: boolean;

    constructor(private api: NxCloudApiService,
                private accountService: NxAccountService,
                private configService: NxConfigService,
                private uriService: NxUriService,
                private menuService: NxMenuService,
                private dialogs: NxDialogsService
    ) {
        this.config = this.configService.getConfig();
    }

    get mergeTarget() {
        return this._mergeTarget;
    }

    set mergeTarget(target) {
        this._mergeTarget = target;
    }

    get system() {
        return this.systemSubject.getValue();
    }

    set system(system) {
        this.systemSubject.next(system);
    }

    setSection(section) {
        this.selectedSectionSubject.next(section);
    }

    loadUsers() {
        return this.system.getUsers(true);
    }

    addUser() {
        // Call share dialog, run process inside
        this.system.pauseUpdate = true;
        return this.dialogs
                   .addUser(this.accountService, this.system)
                   .then((userId) => {
                       if (userId) {
                           userId = this.system.mediaserver.cleanId(userId);
                           this.menuService.setDetailsSection(userId);
                           this.uriService.updateURI(`systems/${this.system.id}/users/${userId}`);
                       }
                   }, (reason) => {
                       // dialog was dismissed ... this handler is required if dialog is dismissible
                       // if we don't handle it will raise a JS error
                       // ERROR Error: Uncaught (in promise): [object Number]
                   })
                   .finally(() => this.system.pauseUpdate = false);
    }

    ngOnDestroy() {
        this.systemSubject.unsubscribe();
    }
}
