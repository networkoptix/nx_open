import { Location }                                               from '@angular/common';
import { Component, HostListener, Inject }                        from '@angular/core';
import { CookieService }                                          from 'ngx-cookie-service';
import { DeviceDetectorService }                                  from 'ngx-device-detector';
import { Title }                                                  from '@angular/platform-browser';
import { ActivatedRoute, ActivationStart, Event, Router } from '@angular/router';
import { filter, debounceTime, timeout, finalize } from 'rxjs/operators';
import { WINDOW }                    from './src/services/window-provider';
import { NxLanguageProviderService } from './src/services/nx-language-provider';
import { NxConfigService }           from './src/services/nx-config';
import { NxApplyService }            from './src/services/apply.service';
import { NxQueryParamService }       from './src/services/query-param.service';
import { NxRibbonService }           from './src/components/ribbon/ribbon.service';
import { NxAppStateService }         from './src/services/nx-app-state.service';
import { fromEvent, Subscription } from 'rxjs';
import 'rxjs-compat/add/observable/fromEvent';
import { NxScrollMechanicsService } from './src/services/scroll-mechanics.service';
import { NxUriService } from './src/services/uri.service';
import { NxPageService } from './src/services/page.service';
import { NxSystemRole } from './src/services/system.service';

@Component({
    selector: 'nx-app',
    template: `        
        <div class="outerContainer" *ngIf="appStateService.ready">
            <div class="headerContainer">
                <nx-header></nx-header>
                <nx-ribbon></nx-ribbon>
            </div>
        
            <div class="mainContainer" nxScrollHelper>
                <router-outlet></router-outlet>
                <div ng-view="" ng-model-options="{ updateOn: 'blur' }"></div>
            </div>
        </div>
        <nx-pre-loader type="page" *ngIf="!appStateService.ready"></nx-pre-loader>
        <app-toasts aria-live="polite" aria-atomic="true"></app-toasts>
    `
})

export class AppComponent {
    CONFIG: any;
    LANG: any;
    deviceInfo: any;
    allowedDevices: {};
    hlsIsSupported: boolean;
    isInIframe: boolean;

    eventSubscription: Subscription;

    constructor(private cookieService: CookieService,
                private deviceService: DeviceDetectorService,
                private location: Location,
                private config: NxConfigService,
                private language: NxLanguageProviderService,
                private applyService: NxApplyService,
                private appStateService: NxAppStateService,
                private scrollMechanicsService: NxScrollMechanicsService,
                private queryParamService: NxQueryParamService,
                private router: Router,
                private ribbonService: NxRibbonService,
                private uriService: NxUriService,
                private pageService: NxPageService,
                @Inject(WINDOW) private window: Window,
    ) {

        this.CONFIG = this.config.getConfig();

        // Allows 3 seconds for auth query param to be detected and set appstate.ready to false.
        // This makes sure only the preloader is shown before the page is refreshed to a logged in state.
        // After 3 seconds we unsubscribe to make sure we don't change the ready state while the app is already loaded
        const authUriSub = this.uriService.getURI()
            .pipe(timeout(3000), finalize(() => authUriSub.unsubscribe()))
            .subscribe(params => {
                if (params.auth) {
                    authUriSub.unsubscribe();
                }
                this.appStateService.ready = !params.auth;
            }, () => {});

        this.scrollMechanicsService.setWindowSize(window.innerHeight, window.innerWidth);

        // TODO: Componentize this
        this.allowedDevices = {
            windows: {
                ie     : 10,
                safari : 10,
                chrome : 64,
                firefox: 60
            },
            mac    : {
                safari : 10,
                chrome : 64,
                firefox: 60
            },
            linux  : {
                chrome : 64,
                firefox: 60
            }
        };

        this.deviceInfo = this.deviceService.getDeviceInfo();
        let allowedDevice = this.allowedDevices[this.deviceInfo.os.toLowerCase()];

        // Special case for Kyle's robot tests
        // ... device detector doesn't detect it correctly
        if (this.deviceInfo.userAgent.indexOf('HeadlessChrome') > -1) {
            allowedDevice = undefined;
        }

        if (allowedDevice !== undefined) {
            const allowedVersion = allowedDevice[this.deviceInfo.browser.toLowerCase()] || 0;
            const majorVersion = this.deviceInfo.browser_version.split('.')[0];

            if (majorVersion < allowedVersion) {
                // redirect
                this.location.go('/browser');
            }
        } // else -> unknown platform or device ... cross fingers and hope for the best

        // this language will be used as a fallback when a translation
        // isn't found in the current language
        this.language.setDefaultLang('en_US');

        // @ts-ignore
        this.language.setTranslations(window.LANG.ajs.language, window.LANG.i18n);
        this.LANG = this.language.getTranslations();
        this.pageService.setLanguage(this.LANG); // during the init of the service LANG is undefined
        // @ts-ignore
        this.pageService.setPageTitle(this.LANG.pageTitles.default);

        // extend CONFIG ... arghhh ugly // @ts-ignore ... no implementation for // @ts-ignore-start/end
        // This was done every time a system is created. Its only need once
        this.CONFIG.accessRoles.predefinedRoles.forEach((option: NxSystemRole) => {
            if (option.permissions) {
                option.permissions = option.permissions.split('|').sort().join('|');
            }
        });
        // @ts-ignore
        this.CONFIG.companyLink = window.SETTINGS.companyLink;
        // @ts-ignore
        this.CONFIG.companyName = window.SETTINGS.companyName;
        // @ts-ignore
        this.CONFIG.copyrightYear = window.SETTINGS.copyrightYear;
        // @ts-ignore
        this.CONFIG.feedbackEnabled = window.SETTINGS.feedbackEnabled;
        // @ts-ignore
        this.CONFIG.footerItems = window.SETTINGS.footerItems;
        // @ts-ignore
        this.CONFIG.integrationFilterItems = window.SETTINGS.integrationFilterItems;
        // @ts-ignore
        this.CONFIG.integrationFilterLimitation = window.SETTINGS.integrationFilterLimitation;
        // @ts-ignore
        this.CONFIG.integrationStoreEnabled = window.SETTINGS.integrationStoreEnabled;
        // @ts-ignore
        this.CONFIG.healthMonitoringEnabled = window.SETTINGS.healthMonitoringEnabled;
        // @ts-ignore
        this.CONFIG.publicDownloads = window.SETTINGS.publicDownloads;
        // @ts-ignore
        this.CONFIG.publicReleases = window.SETTINGS.publicReleases;
        // @ts-ignore
        this.CONFIG.trafficRelayHost = window.SETTINGS.trafficRelayHost;
        // @ts-ignore
        this.CONFIG.supportLink = window.SETTINGS.supportLink;
        // @ts-ignore
        this.CONFIG.privacyLink = window.SETTINGS.privacyLink;
        // @ts-ignore
        this.CONFIG.cloudName = window.SETTINGS.cloudName;
        // @ts-ignore
        this.CONFIG.vmsName = window.SETTINGS.vmsName;
        // @ts-ignore
        this.CONFIG.ipvd.sortSupportedDevicesByPopularity = window.SETTINGS.sortSupportedDevicesByPopularity;
        // @ts-ignore
        this.CONFIG.ipvd.supportedResolutions = window.SETTINGS.supportedResolutions;
        // @ts-ignore
        this.CONFIG.ipvd.supportedHardwareTypes = window.SETTINGS.supportedHardwareTypes;
        // @ts-ignore
        this.CONFIG.ipvd.searchTags = window.SETTINGS.searchTags;
        // @ts-ignore
        this.CONFIG.testedOperatingSystems = window.SETTINGS.testedOperatingSystems;
        // @ts-ignore
        this.CONFIG.googleTagManagerId = window.SETTINGS.googleTagManagerId;
        // @ts-ignore
        if (window.SETTINGS.appTypesForPlatform) {
            // @ts-ignore
            Object.entries(window.SETTINGS.appTypesForPlatform).forEach(([platform, appTypes]: [string, any]) => {
                if (platform in this.CONFIG.downloads.groups && appTypes) {
                    this.CONFIG.downloads.groups[platform].appTypes = appTypes;
                }
            });
        }
        // @ts-ignore
        this.CONFIG.ipvd.vendorsShown = parseInt(window.SETTINGS.vendorsShown);
        // @ts-ignore
        this.CONFIG.pushConfig = window.SETTINGS.pushConfig;

        // @ts-ignore
        if (window.SETTINGS.cloudMerge) {
            // @ts-ignore
            this.CONFIG.cloudMerge = window.SETTINGS.cloudMerge;
        }
        // @ts-ignore
        this.CONFIG.viewsDir = 'static/lang_' + window.LANG.ajs.language + '/views/';
        // @ts-ignore
        this.CONFIG.viewsDirCommon = 'static/lang_' + window.LANG.ajs.language + '/web_common/views/';

        // detect preview mode
        if (window.location.href.indexOf('preview') >= 0) {
            this.CONFIG.previewPath = 'preview';
            this.CONFIG.viewsDir = this.CONFIG.previewPath + '/' + this.CONFIG.viewsDir;
        }

        this.CONFIG.showHeaderAndFooter = true; // Default state

        // (Smart check) Check if page is displayed inside an iframe
        // this.isInIframe = (window.location !== window.parent.location);

        // Route check if page is displayed inside an iframe
        this.isInIframe = (window.location.pathname.indexOf('/embed') === 0);
        if (this.isInIframe) {
            this.appStateService.setHeaderVisibility(false);
            this.appStateService.setFooterVisibility(false);
        }

        // Updates query params for components without routes.
        this.router.events.pipe(
            filter((event: Event) => event instanceof ActivationStart)
        ).subscribe(({ snapshot: { queryParams } }: ActivationStart) => {
            this.queryParamService.queryParams = queryParams;
        });

        fromEvent(window, 'resize').pipe(debounceTime(100)).subscribe((event: any) => {
            this.scrollMechanicsService.setWindowSize(event.target.innerHeight, event.target.innerWidth);
        });
    }

    // Todo: Revisit using this when the hybrid app is killed.
    @HostListener('window:popstate')
    windowListener() {
        if (this.applyService.locked) {
            window.history.go(1);
            this.applyService.showDialog().catch(() => {});
        }
    }
}
