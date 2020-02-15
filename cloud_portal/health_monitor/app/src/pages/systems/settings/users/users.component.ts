import {
    Component, Inject, OnDestroy,
    OnInit, ViewContainerRef
}                                    from '@angular/core';
import { Location }                  from '@angular/common';
import { ActivatedRoute }            from '@angular/router';
import { filter }                    from 'rxjs/operators';

import { NxConfigService }           from '../../../../services/nx-config';
import { NxPageService }             from '../../../../services/page.service';
import { NxDialogsService }          from '../../../../dialogs/dialogs.service';
import { NxSettingsService }         from '../settings.service';
import { NxLanguageProviderService }            from '../../../../services/nx-language-provider';
import { NxMenuService }                        from '../../../../components/menu/menu.service';
import { NxAccountService }                     from '../../../../services/account.service';
import { NxProcessService }                     from '../../../../services/process.service';
import { NxSystem, NxSystemRole, NxSystemUser } from '../../../../services/system.service';
import { NxApplyService, Watcher }              from '../../../../services/apply.service';
import { NxUriService }                         from '../../../../services/uri.service';
import { Subscription }                         from 'rxjs';
import { NxToastService }                       from '../../../../dialogs/toast.service';
import { AutoUnsubscribe } from 'ngx-auto-unsubscribe';

@AutoUnsubscribe()
@Component({
    selector   : 'nx-system-user-component',
    templateUrl: 'users.component.html',
    styleUrls  : ['users.component.scss'],
})

export class NxSystemUsersComponent implements OnInit, OnDestroy {
    CONFIG: any = {};
    LANG: any = {};
    location: any;
    paramUser: any;
    accessDescription: string;
    editUser: any;
    locked: any;
    nextUserId: string;
    selectedUser: NxSystemUser;
    systemAvailable: boolean;
    system: NxSystem;
    viewContainerRef: ViewContainerRef;

    userEnabled = new Watcher<boolean>();
    userRole = new Watcher<string>();

    private routeParamsSubscription: Subscription;
    private systemSubscription: Subscription;
    private userSubscription: Subscription;

    private setupDefaults() {
        this.CONFIG = this.configService.getConfig();
        this.locked = {};
        this.menuService.setSection('users');
    }

    constructor(@Inject(ViewContainerRef) viewContainerRef,
                private route: ActivatedRoute,
                private accountService: NxAccountService,
                private applyService: NxApplyService,
                private configService: NxConfigService,
                private language: NxLanguageProviderService,
                private pageService: NxPageService,
                private dialogs: NxDialogsService,
                private settingsService: NxSettingsService,
                private menuService: NxMenuService,
                private processService: NxProcessService,
                private uriService: NxUriService,
                private toastService: NxToastService,
                location: Location) {
        this.location = location;
        this.viewContainerRef = viewContainerRef;
        this.setupDefaults();
    }

    ngOnInit(): void {
        this.LANG = this.language.getTranslations();

        this.settingsService.share = this.route.snapshot.routeConfig.path === 'share';

        this.routeParamsSubscription = this.route
            .params
            .subscribe(params => {
                if (params.userId) {
                    this.menuService.setDetailsSection(params.userId);
                    this.paramUser = params.userId;
                    this.setUser();
                }
            });

        this.systemSubscription = this.settingsService.systemSubject
            .pipe(filter(data => data !== undefined))
            .subscribe((system) => {
                this.system = system;
                this.pageService.setPageTitle(this.LANG.pageTitles.systemName.replace('{{systemName}}', this.system.info.name));
                // Route guard did not worked :( ... so doing it the old way
                if (!this.system.permissions || !this.system.permissions.editUsers) {
                    this.uriService.updateURI('systems/' + this.system.id, {});
                    return;
                }
                if (this.userSubscription) {
                    this.userSubscription.unsubscribe();
                }
                this.userSubscription = this.system.infoSubject.subscribe(() => {
                    this.systemAvailable = this.system.isAvailable && this.system.mergeInfo === undefined;
                    if (!this.applyService.locked && !this.system.pauseUpdate) {
                        this.setUser();
                    }
                });
            });

        this.initProcesses();

        this.applyService.initPageWatcher(this.viewContainerRef, this.editUser, () => {
            this.selectedUser.isEnabled = this.userEnabled.originalValue;
            this.selectedUser.role = this.system.accessRoles.find(role => role.name === this.userRole.originalValue);
            this.applyService.reset();
        }, [this.userEnabled, this.userRole]);
    }

    ngOnDestroy(): void {}

    initProcesses(): void {
        this.editUser = this.processService.createProcess(() => {
            const selectedUser = this.selectedUser;
            if (this.locked[selectedUser.email]) {
                return;
            }
            this.locked[selectedUser.email] = true;
            return this.system.saveUser(selectedUser, selectedUser.role).then(() => {
                return this.system.getUsers(true);
            }).then(() => {
                this.locked[selectedUser.email] = false;
                return;
            });
        }, {}).then(() => {
            setTimeout(() => {
                this.applyService.hardReset();
                this.setUser();
                this.applyService.reset();
            });
        });
    }

    removeUser() {
        const user = this.selectedUser;
        if (this.locked[user.email]) {
            return;
        }
        this.locked[user.email] = true;
        this.calcNextUserId();

        this.dialogs.removeUser(this.system, user).then((result) => {
            if (result) {
                this.applyService.reset();
                delete this.locked[user.email];
                this.uriService.updateURI(`systems/${this.system.id}/users/${this.nextUserId}`);
                this.menuService.setDetailsSection(this.nextUserId);
            } else {
                this.locked[user.email] = false;
            }
        });
    }

    calcNextUserId () {
        const currentUserIndex = this.system.users.findIndex((user) => {
            return user.id === this.selectedUser.id;
        });
        const nextUserIndex = currentUserIndex + 1 !== this.system.users.length ? currentUserIndex + 1 : currentUserIndex - 1;
        this.nextUserId = this.system.mediaserver.cleanId(this.system.users[nextUserIndex].id);
    }

    setUser() {
        if (this.system && this.system.users.length > 0) {
            let user;
            if (this.paramUser) {
                 user = this.system.users.find((user: any) => {
                    return user.id.replace(/{|}/g, '') === this.paramUser;
                });
            }
            if (typeof(user) === 'undefined') {
                user = this.system.users[0];
                const userId = this.system.mediaserver.cleanId(user.id);
                this.uriService.updateURI(`systems/${this.system.id}/users/${userId}`);
            }

            // If there's no users skip setting section and permissions
            if (typeof(user) === 'undefined') {
                return;
            }
            this.applyService.hardReset();
            this.selectedUser = {... user};
            this.menuService.setDetailsSection(this.selectedUser.id.replace(/{|}/g, ''));
            this.setPermission(this.selectedUser.role);
            this.userEnabled.value = this.selectedUser.isEnabled;
            this.applyService.reset();

            this.settingsService.footerSubject.next(true);
            setTimeout(() => this.applyService.setVisible(this.selectedUser.canBeEdited));
        }
    }

    setPermission(role: NxSystemRole|any) {
        const userRole = role && role.name ? role.name : this.selectedUser.accessRole;
        this.accessDescription = this.LANG.accessRoles[userRole] ?
                this.LANG.accessRoles[userRole].description :
                this.LANG.accessRoles.customRole.description;
        this.selectedUser.role = role;
        this.userRole.value = role.name;
    }

    updateEnabled(state) {
        this.selectedUser.isEnabled = state;
        this.userEnabled.value = state;
    }
}

