import { NgModule }                                                       from '@angular/core';
import { Location, PathLocationStrategy, LocationStrategy, CommonModule } from '@angular/common';
import { BrowserModule }                                                  from '@angular/platform-browser';
import { downgradeComponent, downgradeInjectable, UpgradeModule }         from '@angular/upgrade/static';
import { RouterModule, UrlHandlingStrategy, UrlTree }                     from '@angular/router';
import { HttpClientModule }                                               from '@angular/common/http';

import { FormsModule } from '@angular/forms';

import { NgbModule }            from '@ng-bootstrap/ng-bootstrap';
import { NgbModal }             from '@ng-bootstrap/ng-bootstrap';
import { OrderModule }          from 'ngx-order-pipe';
import { DeviceDetectorModule } from 'ngx-device-detector';

import { CoreModule } from './core/index';

import { cloudApiServiceModule } from './ajs-upgraded-providers';
import { systemsModule }         from './ajs-upgraded-providers';
import { languageServiceModule } from './ajs-upgraded-providers';
import { accountServiceModule }  from './ajs-upgraded-providers';
import { processServiceModule }  from './ajs-upgraded-providers';
import { uuid2ServiceModule }    from './ajs-upgraded-providers';
import { ngToastModule }         from './ajs-upgraded-providers';
import { configServiceModule }   from './ajs-upgraded-providers';

import { AppComponent }                                 from './app.component';
import { BarModule }                                    from './bar/bar.module';
import { DownloadModule }                               from './download/download.module';
import { DropdownsModule }                              from './dropdowns/dropdowns.module';
import { NxModalLoginComponent, LoginModalContent }     from "./dialogs/login/login.component";
import { NxProcessButtonComponent }                     from './components/process-button/process-button.component';
import { nxDialogsService }                             from "./dialogs/dialogs.service";
import { GeneralModalContent, NxModalGeneralComponent } from "./dialogs/general/general.component";

class HybridUrlHandlingStrategy implements UrlHandlingStrategy {
    // use only process the `/bar` url
    shouldProcessUrl(url: UrlTree) {
        return url.toString().startsWith('/bar') ||
            url.toString().startsWith('/download');
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
        UpgradeModule,
        HttpClientModule,
        FormsModule,
        OrderModule,
        CoreModule,
        BarModule,
        DownloadModule,
        cloudApiServiceModule,
        uuid2ServiceModule,
        languageServiceModule,
        accountServiceModule,
        processServiceModule,
        systemsModule,
        ngToastModule,
        configServiceModule,
        DropdownsModule,

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

