import {
    Component, Inject, OnDestroy, OnInit,
    Renderer2
} from '@angular/core';
import {
    ActivatedRoute, NavigationEnd, Event,
    Router, RoutesRecognized
}                              from '@angular/router';
import { NxConfigService }        from '../../services/nx-config';
import { NxAppStateService }      from '../../services/nx-app-state.service';
import { NxAccountService }       from '../../services/account.service';
import { NxDialogsService }       from '../../dialogs/dialogs.service';
import { NxSessionService }       from '../../services/session.service';
import { NxSystemsService }       from '../../services/systems.service';
import { WINDOW }                 from '../../services/window-provider';
import { LocalStorageService }    from 'ngx-store';
import { Subscription, timer }    from 'rxjs';
import { NxHeaderService }        from '../../services/nx-header.service';
import { NxSystem, NxSystemService } from '../../services/system.service';
import { NxLanguageProviderService } from '../../services/nx-language-provider';
import { AutoUnsubscribe } from 'ngx-auto-unsubscribe';

@AutoUnsubscribe()
@Component({
    selector: 'nx-header',
    templateUrl: 'header.component.html',
    styleUrls: [ 'header.component.scss' ]
})
export class NxHeaderComponent implements OnInit, OnDestroy {

    CONFIG: any = {};
    LANG: any = {};

    user: any = {};
    canSeeInfo: boolean;
    system: NxSystem;
    systems: any;
    systemId: any;
    active: any = {};
    activeSystem: any = {};
    singleSystem: any = {};
    inline: any;
    navVisible: boolean;
    dropdownsVisible: boolean;
    viewHeader: boolean;
    systemCounter: number;
    loginState: any;

    getUrlSystemId: any;
    untilHaveID: any;
    private headerSubscription: Subscription;
    private infoSubscription: Subscription;
    private loginSubscription: Subscription;
    private routerSubscription: Subscription;
    private systemSubscription: Subscription;
    private systemIdSubscription: Subscription;

    constructor(@Inject(WINDOW) private window: Window,
                private renderer: Renderer2,
                private _config: NxConfigService,
                private appState: NxAppStateService,
                private route: ActivatedRoute,
                private systemsService: NxSystemsService,
                private systemService: NxSystemService,
                private dialogs: NxDialogsService,
                private languageService: NxLanguageProviderService,
                private accountService: NxAccountService,
                private sessionService: NxSessionService,
                private localStorage: LocalStorageService,
                private router: Router,
                private headerService: NxHeaderService,
    ) {
        this.CONFIG = this._config.getConfig();
        this.LANG = this.languageService.getTranslations();
    }

    private systemIdUpdate(id) {
        this.systemId = id;
        this.localStorage.set('systemId', this.systemId);

        if (this.systemId && !this.systems) {
            this.systemsService
                .forceUpdateSystems()
                .toPromise().then(() => {
                    this.updateActiveSystem();
                    this.updateActive();
            });
        } else {
            this.updateActiveSystem();
            this.updateActive();
        }
    }

    private startTimerSystemIdUpdate() {
        this.untilHaveID = timer(200, 200);
        this.getUrlSystemId = this.untilHaveID.subscribe(() => {
            if (window.location.pathname.indexOf('/systems/') === 0) {
                const uriSystemId = window.location.pathname.split('/')[2];

                if (uriSystemId === this.systemId) {
                    this.getUrlSystemId.unsubscribe();
                    return;
                }

                this.systemIdUpdate(uriSystemId);
            }
        });
    }

    private stopActiveSubscription() {
        if (this.infoSubscription && !this.infoSubscription.closed) {
            this.infoSubscription.unsubscribe();
            this.system.stopPoll();
            this.system = undefined;
        }
    }

    ngOnDestroy() {}

    ngOnInit() {
        // TODO: root route is maintained by AJS - replace this once we get rid of it.
        this.inline = this.window.location.search.indexOf('inline') > 0;
        // this.route.queryParams.subscribe(params => {
        //     this.inline = params['inline'] !== 'undefined';
        // });

        // TODO: (Only for display purpose) Temporary solution until we move View to A8
        // View is still under AJS and it doesn't trigger route change
        this.startTimerSystemIdUpdate(); // ensure update on page reload

        // notification from view.js
        this.systemIdSubscription = this.headerService.systemIdSubject.subscribe((systemId) => {
            if (systemId) {
                this.systemIdUpdate(systemId);
            }
        });
        // TODO: END

        // TODO: experiment iFrame
        // this.headerService.visibilitySubject.subscribe((state) => {
        //     if (state !== undefined) {
        //         this.viewHeader = state;
        //     }
        // });

        this.navVisible = false;
        this.dropdownsVisible = false;
        this.viewHeader = this.CONFIG.showHeaderAndFooter;
        this.active = {};

        this.headerSubscription = this.appState.headerVisibleSubject.subscribe((visible) => {
            this.viewHeader = visible;
        });

        this.routerSubscription = this.router.events
              .subscribe((event: Event) => {
                  if (event instanceof RoutesRecognized) {
                      this.systemId = event.state.root.firstChild.params.systemId || '';
                      this.localStorage.set('systemId', this.systemId);
                      this.updateActiveSystem();
                      this.updateActive();
                  }

                  if (event instanceof NavigationEnd) {
                      // You only receive NavigationEnd events
                      if (this.systemId && !this.systems) {
                          this.systemsService
                              .forceUpdateSystems()
                              .toPromise().then(() => {
                                  this.updateActiveSystem();
                                  this.updateActive();
                              });
                      } else {
                          this.updateActiveSystem();
                          this.updateActive();
                      }
                  }
              });

        this.loginSubscription = this.sessionService.loginStateSubject.subscribe((loginState) => {
            this.accountService
                .get()
                .then(account => {
                    this.renderer.removeClass(document.body, 'loading');

                    if (account) {
                        this.dropdownsVisible = true;
                        this.systemsService.getSystem(account.email);

                        this.loginState = true;
                        this.renderer.removeClass(document.body, 'anonymous');
                        this.renderer.addClass(document.body, 'authorized');
                        this.systemsService
                            .forceUpdateSystems(loginState)
                            .toPromise()
                            .then(() => this.updateActive());
                    } else {
                        this.loginState = false;
                        this.renderer.removeClass(document.body, 'authorized');
                        this.renderer.addClass(document.body, 'anonymous');
                    }
                });
        });

        this.systemSubscription = this.systemsService.systemsSubject.subscribe((systems) => {
            if (!systems) {
                return;
            }

            this.systemId = this.localStorage.get('systemId');

            if (!this.systemId && this.route.firstChild && this.route.firstChild.snapshot.params.systemId) {
                this.systemId = this.route.firstChild.snapshot.params.systemId;
            }
            this.systems = systems;
            this.singleSystem = (this.systems.length === 1);
            this.systemCounter = this.systems.length;

            this.updateActiveSystem();
            this.updateActive();
        });
    }

    private isActive(val) {
        return this.window.location.pathname.indexOf(val) >= 0;
    }

    onClick(event) {
        if (this.systemId && this.isActive(event.target.id) && !this.isActive('view') && !this.isActive('health')) {
            event.stopPropagation();
            return false;
        } else {
            if (event.target.id === 'systems') {
                return true;
            }
        }
        if (this.isActive(event.target.id)) {
            event.stopPropagation();
            return false;
        }
    }

    login () {
        const url = this.window.location.pathname;
        const redirect = this.CONFIG.redirectPaths.some((path) => url.indexOf(path) > -1);
        // Handling promise to satisfy the linter.
        this.dialogs.login(this.accountService, !redirect).then(() => {});
    }

    logout () {
        this.accountService.logout(true);
    }

    updateActive() {
        this.active.ipvd = this.isActive('/ipvd');
        this.active.integrations = this.isActive('/integrations');
        this.active.register = this.isActive('/register');
        this.active.view = this.isActive('/view');
        this.active.information = this.isActive('/health');
        this.active.settings = this.systemId && this.isActive('/systems') && !this.isActive('/view') && !this.isActive('/health');
        this.navVisible = true;
    }

    updateActiveSystem() {
        if (!this.systems) {
            return;
        }
        if (this.systemId) {
            if (this.singleSystem) { // Special case for a single system - it always active
                this.activeSystem = this.systems[0];
            } else {
                this.activeSystem = this.systems.find((system) => {
                    return this.systemId === system.id;
                });
            }
        } else {
            this.activeSystem = undefined;
        }

        this.accountService
            .get()
            .then(account => {
                if (account) {
                    this.user = account;
                    if (this.activeSystem) {
                        if (!this.system || this.system.id !== this.systemId) {
                            this.stopActiveSubscription();
                            this.system = this.systemService.createSystem(this.user.email, this.activeSystem.id);

                            this.system.getInfoAndPermissions(false).catch(_ => {}).then(system => {
                                this.canSeeInfo = (this.CONFIG.healthMonitoringEnabled || system.info.capabilities && system.info.capabilities.vms_metrics) && this.system.canViewInfo();
                            });
                        }
                    } else {
                        this.stopActiveSubscription();
                    }
                }
            });
        }
}
