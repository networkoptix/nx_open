import { Component, OnDestroy, OnInit } from '@angular/core';
import { Location }          from '@angular/common';
import { NxConfigService }   from '../../../services/nx-config';
import { NxAccountService }  from '../../../services/account.service';
import { NxSessionService }  from '../../../services/session.service';
import { Subscription } from 'rxjs';
import { AutoUnsubscribe } from 'ngx-auto-unsubscribe';

@AutoUnsubscribe()
@Component({
    selector: 'nx-account-settings-select',
    templateUrl: 'account-settings.component.html',
    styleUrls: ['account-settings.component.scss']
})

export class NxAccountSettingsDropdown implements OnInit, OnDestroy {
    config: any;
    settings = {
        email: '',
        is_staff: false,
        is_superuser: false
    };
    show: boolean;
    private loginSubscription: Subscription;

    constructor(private accountService: NxAccountService,
                private _config: NxConfigService,
                private sessionService: NxSessionService,
                private location: Location) {
        this.config = this._config.getConfig();
        this.show = false;
    }

    ngOnDestroy() {}

    ngOnInit()  {
        this.getAccount();
        this.loginSubscription = this.accountService.loginStateSubject
            .subscribe(() => {
                this.getAccount();
            });
    }

    getAccount() {
        this.accountService
            .get()
            .then(account => {
                if (account) {
                    this.settings.email = account.email;
                    this.settings.is_staff = account.is_staff;
                    this.settings.is_superuser = account.is_superuser;
                }
            });
    }

    logout(): void {
        const url = this.location.path();
        const stay = url.startsWith('/systems') ||
                     url.startsWith('/account') ||
                     url.startsWith('/push-notifications') ||
                     url.startsWith('/download') && !this.config.publicDownloads  ||
                     url.startsWith('/downloads') && !this.config.publicReleases;
        this.accountService.logout(!stay);
    }
}
