import { NgModule }                                                       from '@angular/core';
import { Location, PathLocationStrategy, LocationStrategy, CommonModule } from '@angular/common';
import { BrowserModule }                                                  from '@angular/platform-browser';
import { BrowserAnimationsModule }                                        from '@angular/platform-browser/animations';
import { RouterModule, UrlHandlingStrategy, UrlTree }                     from '@angular/router';
import { HttpClient, HttpClientModule }                                   from '@angular/common/http';

import { NgbModule, NgbModal }                                from '@ng-bootstrap/ng-bootstrap';
import { OrderModule }                                        from 'ngx-order-pipe';
import { DeviceDetectorModule }                               from 'ngx-device-detector';
import { TranslateModule, TranslateService, TranslateLoader } from '@ngx-translate/core';
import { TranslateHttpLoader }                                from '@ngx-translate/http-loader';
import { CookieService }                                      from 'ngx-cookie-service';

import {
    cloudApiServiceModule, systemModule, systemsModule, languageServiceModule,
    accountServiceModule, processServiceModule, uuid2ServiceModule,
    ngToastModule, configServiceModule, authorizationCheckServiceModule,
    localStorageModule, locationProxyModule
} from './src/ajs-upgrade/ajs-upgraded-providers';

import { AppComponent }        from './app.component';
import { DropdownsModule }     from './src/dropdowns/dropdowns.module';
import { DialogsModule }       from './src/dialogs/dialogs.module';
import { PagesModule }         from './src/pages/pages.module';
import { DirectivesModule }    from './src/directives/directives.module';
import { downgradeInjectable } from '@angular/upgrade/static';
import { ServiceModule }    from './src/services/services.module';

// AoT requires an exported function for factories
export function createTranslateLoader(http: HttpClient) {
    return new TranslateHttpLoader(http, 'static/lang_', '/language_i18n.json');
}

class HybridUrlHandlingStrategy implements UrlHandlingStrategy {
    shouldProcessUrl(url: UrlTree) {
        return url.toString().startsWith('/sandbox');
        // return false;
        // url.toString().startsWith('/download') ||
        // url.toString().startsWith('/downloads') ||
        // url.toString().startsWith('/browser');
    }

    extract(url: UrlTree) {
        return url;
    }

    merge(url: UrlTree, whole: UrlTree) {
        return url;
    }
}

@NgModule({
    imports        : [
        CommonModule,
        BrowserModule,
        BrowserAnimationsModule,
        HttpClientModule,
        OrderModule,
        cloudApiServiceModule,
        uuid2ServiceModule,
        localStorageModule,
        languageServiceModule,
        accountServiceModule,
        processServiceModule,
        systemModule,
        systemsModule,
        ngToastModule,
        configServiceModule,
        authorizationCheckServiceModule,
        locationProxyModule,
        DropdownsModule,
        DialogsModule,
        ServiceModule,
        PagesModule,
        DirectivesModule,

        TranslateModule.forRoot({
            loader: {
                provide   : TranslateLoader,
                useFactory: (createTranslateLoader),
                deps      : [ HttpClient ]
            }
        }),
        DeviceDetectorModule.forRoot(),
        NgbModule.forRoot(),
        RouterModule.forRoot([], { initialNavigation: true })
    ],
    entryComponents: [],
    providers      : [
        NgbModal,
        Location,
        CookieService,
        TranslateService,
        { provide: LocationStrategy, useClass: PathLocationStrategy },
        { provide: UrlHandlingStrategy, useClass: HybridUrlHandlingStrategy },
    ],
    declarations   : [
        AppComponent
    ],
    bootstrap      : [ AppComponent ]
})

export class AppModule {
    ngDoBootstrap() {
    }
}

