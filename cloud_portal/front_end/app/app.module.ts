import { NgModule } from '@angular/core';
import { Location, PathLocationStrategy, LocationStrategy, CommonModule } from '@angular/common';
import { BrowserModule } from '@angular/platform-browser';
import { downgradeComponent, UpgradeModule } from '@angular/upgrade/static';
import { RouterModule, UrlHandlingStrategy, UrlTree, Routes } from '@angular/router';
import { HttpClientModule } from '@angular/common/http';

import { FormsModule } from '@angular/forms';

import { NgbModule } from '@ng-bootstrap/ng-bootstrap';
import { NgbModal } from '@ng-bootstrap/ng-bootstrap';

import { CoreModule } from './core/index';

import { cloudApiServiceProvider } from './ajs-upgraded-providers';
import { languageServiceModule } from './ajs-upgraded-providers';
import { accountServiceModule } from './ajs-upgraded-providers';
import { processServiceModule } from './ajs-upgraded-providers';
import { uuid2ServiceModule } from './ajs-upgraded-providers';

import { AppComponent } from './app.component';
import { BarModule } from './bar/bar.module';
import { NxLanguageDropdown } from "./dropdown/language.component";
import { NxModalLoginComponent } from "./dialogs/login/login.component";
import { NxProcessButtonComponent } from './components/process-button/process-button.component';

class HybridUrlHandlingStrategy implements UrlHandlingStrategy {
    // use only process the `/bar` url
    shouldProcessUrl(url: UrlTree) {
        return url.toString().startsWith('/bar');
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
        CoreModule,
        BarModule,
        uuid2ServiceModule,
        languageServiceModule,
        accountServiceModule,
        processServiceModule,

        NgbModule.forRoot(),
        RouterModule.forRoot([], { initialNavigation: false })
    ],
    entryComponents: [
        NxLanguageDropdown,
        NxModalLoginComponent,
        NxProcessButtonComponent
    ],
    providers: [
        NgbModal,
        Location,
        { provide: LocationStrategy, useClass: PathLocationStrategy },
        { provide: UrlHandlingStrategy, useClass: HybridUrlHandlingStrategy },
        { provide: '$scope', useFactory: i => i.get('$rootScope'), deps: ['$injector'] },
        cloudApiServiceProvider
    ],
    declarations: [
        AppComponent
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
        .directive('nxLanguageSelect', downgradeComponent({ component: NxLanguageDropdown }) as angular.IDirectiveFactory)
        .directive('nxModalLogin', downgradeComponent({ component: NxModalLoginComponent }) as angular.IDirectiveFactory);



