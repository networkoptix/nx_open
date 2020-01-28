import { Component, OnDestroy, OnInit } from '@angular/core';
import { Location }                             from '@angular/common';
import { ActivatedRoute, Router }               from '@angular/router';
import { NxConfigService }                      from '../../../services/nx-config';
import { NxLanguageProviderService }            from '../../../services/nx-language-provider';

import { NxPageService }        from '../../../services/page.service';
import { NxDialogsService }     from '../../../dialogs/dialogs.service';
import { NxSystemsService }     from '../../../services/systems.service';
import { NxAccountService }     from '../../../services/account.service';
import { NxUrlProtocolService } from '../../../services/url-protocol.service';
import { NxProcessService }     from '../../../services/process.service';
import { debounceTime }         from 'rxjs/operators';
import { Subject, Subscription } from 'rxjs';
import { AutoUnsubscribe } from 'ngx-auto-unsubscribe';

@AutoUnsubscribe()
@Component({
    selector   : 'nx-systems-list-component',
    templateUrl: 'list.component.html',
    styleUrls  : ['list.component.scss']
})

export class NxSystemsListComponent implements OnInit, OnDestroy {
    CONFIG: any = {};
    LANG: any = {};
    showSearch: any;
    fetchComplete: any;
    search: any;
    gettingSystems: any;
    openClient: any;
    systems: any;
    filteredSystems: any;
    userEmail: string;
    searchChanged = new Subject();
    private searchSubscription: Subscription;
    private systemSubscription: Subscription;

    private setupDefaults() {
        this.CONFIG = this.configService.getConfig();
        this.LANG = this.language.getTranslations();

        this.pageService.setPageTitle(this.LANG.pageTitles.systems);
    }

    constructor(
                private urlProtocol: NxUrlProtocolService,
                private route: ActivatedRoute,
                private configService: NxConfigService,
                private language: NxLanguageProviderService,
                private pageService: NxPageService,
                private dialogs: NxDialogsService,
                private systemsService: NxSystemsService,
                private accountService: NxAccountService,
                private processService: NxProcessService,
                private router: Router,
                private location: Location,
    ) {
        this.setupDefaults();
    }

    ngOnInit(): void {
        this.CONFIG = this.configService.getConfig();
        this.showSearch = false;
        this.fetchComplete = false;
        this.search = { value: '' };

        this.accountService.get()
            .then((account) => {
                if (account) {
                    this.userEmail = account.email;
                    this.systemsService.getSystems(account.email);
                }
            });

        this.systemSubscription = this.systemsService.systemsSubject.subscribe((systems) => {
            this.systems = systems;
            if (this.systems === undefined) {
                return;
            }

            if (this.location.path().indexOf('/systems') === 0) {
                if (this.systems.length === 1) {
                    this.openSystem(this.systems[0]);
                }

                this.showSearch = this.systems.length >= this.CONFIG.minSystemsToSearch;

                this.searchSystems();
            }
        });

        this.gettingSystems = this.processService.createProcess(() => {
            this.fetchComplete = true;
            return this.systemsService.forceUpdateSystems().subscribe(_ => {
            });
        }, {
            errorPrefix    : this.LANG.errorCodes.cantGetSystemsListPrefix,
            logoutForbidden: true
        });

        this.searchSubscription = this.searchChanged
            .pipe(debounceTime(this.CONFIG.search.debounceTime))
            .subscribe(() => {
                this.searchSystems();
            });
    }

    trackItem(index, item) {
        if (!item) {
            return undefined;
        }
        return item.id;
    }

    getSystemOwnerName(system, currentEmail) {
        return this.systemsService.getSystemOwnerName(system, currentEmail);
    }

    hasMatch(str, search) {
        return str.toLowerCase().indexOf(search.toLowerCase()) >= 0;
    }

    searchSystems() {
        const search = this.search.value;

        if (search) {
            this.filteredSystems = this.systems.filter((system) => {
                return !search ||
                        this.hasMatch(this.LANG.system.mySystemSearch, search) && (system.ownerAccountEmail === this.accountService.getEmail()) ||
                        this.hasMatch(system.name, search) ||
                        this.hasMatch(system.ownerFullName, search) ||
                        this.hasMatch(system.ownerAccountEmail, search);
            });
        } else {
            this.filteredSystems = this.systems;
        }
    }

    setSearch(value) {
        this.search.value = value;
        this.searchChanged.next();
    }

    openSystem(system) {
        this.router.navigate(['/systems/' + system.id]);
    }

    ngOnDestroy(): void {}

}

