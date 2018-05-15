import { NgModule }                                                       from '@angular/core';
import { Location, PathLocationStrategy, LocationStrategy, CommonModule } from '@angular/common';
import { BrowserModule }                                                  from '@angular/platform-browser';
import { BrowserAnimationsModule }                                        from '@angular/platform-browser/animations';
import { UpgradeModule }                                                  from '@angular/upgrade/static';
import { RouterModule, UrlHandlingStrategy, UrlTree }                     from '@angular/router';
import { HttpClient, HttpClientModule }                                   from '@angular/common/http';

import { NgbModule }                        from '@ng-bootstrap/ng-bootstrap';
import { NgbModal }                         from '@ng-bootstrap/ng-bootstrap';
import { OrderModule }                      from 'ngx-order-pipe';
import { DeviceDetectorModule }             from 'ngx-device-detector';
import { TranslateModule, TranslateLoader } from '@ngx-translate/core';
import { TranslateHttpLoader }              from '@ngx-translate/http-loader';
import { CookieService }                    from "ngx-cookie-service";

import {
    cloudApiServiceModule, systemsModule, languageServiceModule,
    accountServiceModule, processServiceModule, uuid2ServiceModule,
    ngToastModule, configServiceModule, authorizationCheckServiceModule
} from './src/ajs-upgrade/ajs-upgraded-providers';

import { AppComponent }          from './app.component';
import { DownloadModule }        from './src/download/download.module';
import { DownloadHistoryModule } from './src/download-history/download-history.module';
import { DropdownsModule }       from './src/dropdowns/dropdowns.module';
import { DialogsModule }         from './src/dialogs/dialogs.module';

// AoT requires an exported function for factories
export function createTranslateLoader(http: HttpClient) {
    return new TranslateHttpLoader(http, 'static/lang_', '/language_i18n.json');
}

class HybridUrlHandlingStrategy implements UrlHandlingStrategy {
    shouldProcessUrl(url: UrlTree) {
        return url.toString().startsWith('/download') ||
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
        OrderModule,
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
        DialogsModule,

        TranslateModule.forRoot({
            loader: {
                provide: TranslateLoader,
                useFactory: (createTranslateLoader),
                deps: [HttpClient]
            }
        }),
        DeviceDetectorModule.forRoot(),
        NgbModule.forRoot(),
        RouterModule.forRoot([], {initialNavigation: true})
    ],
    entryComponents: [],
    providers: [
        NgbModal,
        Location,
        CookieService,
        {provide: LocationStrategy, useClass: PathLocationStrategy},
        {provide: UrlHandlingStrategy, useClass: HybridUrlHandlingStrategy}
    ],
    declarations: [
        AppComponent,
    ],
    bootstrap: [AppComponent]
})

export class AppModule {
    ngDoBootstrap() {
    }
}

