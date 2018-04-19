import { NgModule }                                                       from '@angular/core';
import { Location, PathLocationStrategy, LocationStrategy, CommonModule } from '@angular/common';
import { BrowserModule }                                                  from '@angular/platform-browser';
import { BrowserAnimationsModule }                                        from '@angular/platform-browser/animations';
import { downgradeComponent, downgradeInjectable, UpgradeModule }         from '@angular/upgrade/static';
import { RouterModule, UrlHandlingStrategy, UrlTree }                     from '@angular/router';
import { HttpClient, HttpClientModule }                                   from '@angular/common/http';

import { FormsModule } from '@angular/forms';

import { NgbModule }                        from '@ng-bootstrap/ng-bootstrap';
import { NgbModal }                         from '@ng-bootstrap/ng-bootstrap';
import { OrderModule }                      from 'ngx-order-pipe';
import { DeviceDetectorModule }             from 'ngx-device-detector';
import { TranslateModule, TranslateLoader } from '@ngx-translate/core';
import { TranslateHttpLoader }              from '@ngx-translate/http-loader';
import { CookieService }                    from "ngx-cookie-service";

import { CoreModule }                      from './src/core/index';
import { cloudApiServiceModule }           from './ajs-upgraded-providers';
import { systemsModule }                   from './ajs-upgraded-providers';
import { languageServiceModule }           from './ajs-upgraded-providers';
import { accountServiceModule }            from './ajs-upgraded-providers';
import { processServiceModule }            from './ajs-upgraded-providers';
import { uuid2ServiceModule }              from './ajs-upgraded-providers';
import { ngToastModule }                   from './ajs-upgraded-providers';
import { configServiceModule }             from './ajs-upgraded-providers';
import { authorizationCheckServiceModule } from './ajs-upgraded-providers';

import { AppComponent }                                 from './app.component';
import { BarModule }                                    from './src/bar/bar.module';
import { DownloadModule }                               from './src/download/download.module';
import { DownloadHistoryModule }                        from './src/download-history/download-history.module';
import { DropdownsModule }                              from './src/dropdowns/dropdowns.module';
import { NxModalLoginComponent, LoginModalContent }     from "./src/dialogs/login/login.component";
import { NxProcessButtonComponent }                     from './src/components/process-button/process-button.component';
import { nxDialogsService }                             from "./src/dialogs/dialogs.service";
import { GeneralModalContent, NxModalGeneralComponent } from "./src/dialogs/general/general.component";

// AoT requires an exported function for factories
export function createTranslateLoader(http: HttpClient) {
    return new TranslateHttpLoader(http, './assets/i18n/', '.json');
}

class HybridUrlHandlingStrategy implements UrlHandlingStrategy {
    shouldProcessUrl(url: UrlTree) {
        return url.toString().startsWith('/bar') ||
            url.toString().startsWith('/download') ||
            url.toString().startsWith('/downloads');
    }

    extract(url: UrlTree) {
        return url;
    }

    merge(url: UrlTree, whole: UrlTree) {
        return url;
    }
}

@NgModule({
    imports: [
        CommonModule,
        BrowserModule,
        BrowserAnimationsModule,
        UpgradeModule,
        HttpClientModule,
        FormsModule,
        OrderModule,
        CoreModule,
        BarModule,
        DownloadModule,
        DownloadHistoryModule,
        cloudApiServiceModule,
        uuid2ServiceModule,
        languageServiceModule,
        accountServiceModule,
        processServiceModule,
        systemsModule,
        ngToastModule,
        configServiceModule,
        authorizationCheckServiceModule,
        DropdownsModule,

        TranslateModule.forRoot({
            loader: {
                provide: TranslateLoader,
                useFactory: (createTranslateLoader),
                deps: [HttpClient]
            }
        }),
        DeviceDetectorModule.forRoot(),
        NgbModule.forRoot(),
        RouterModule.forRoot([], {initialNavigation: false})
    ],
    entryComponents: [
        NxProcessButtonComponent,
        LoginModalContent,
        NxModalLoginComponent,
        GeneralModalContent,
        NxModalGeneralComponent,
    ],
    providers: [
        NgbModal,
        Location,
        CookieService,
        {provide: LocationStrategy, useClass: PathLocationStrategy},
        {provide: UrlHandlingStrategy, useClass: HybridUrlHandlingStrategy},
        // {provide: '$scope', useFactory: i => i.get('$rootScope'), deps: ['$injector']},
        // {provide: '$rootScope', useFactory: i => i.get('$rootScope'), deps: ['$injector']},
        NxModalLoginComponent,
        NxModalGeneralComponent,
        NxProcessButtonComponent,
        nxDialogsService
    ],
    declarations: [
        AppComponent,
        LoginModalContent,
        NxModalLoginComponent,
        GeneralModalContent,
        NxModalGeneralComponent,
        NxProcessButtonComponent,
    ],
    bootstrap: [AppComponent]
})

export class AppModule {
    ngDoBootstrap() {
    }
}

declare var angular: angular.IAngularStatic;
angular
    .module('cloudApp.directives')
    .directive('nxModalLogin', downgradeComponent({component: NxModalLoginComponent}) as angular.IDirectiveFactory);

angular
    .module('cloudApp.services')
    .service('nxDialogsService', downgradeInjectable(nxDialogsService));

