import { NgModule } from '@angular/core';
import { PathLocationStrategy, LocationStrategy, CommonModule } from '@angular/common';
import { BrowserModule } from '@angular/platform-browser';
import { downgradeComponent, UpgradeModule } from '@angular/upgrade/static';
import { RouterModule, UrlHandlingStrategy, UrlTree, Routes } from '@angular/router';
import { HttpClientModule } from '@angular/common/http';

import { NgbModule } from '@ng-bootstrap/ng-bootstrap';

import { CoreModule } from './core/index';
import { cloudApiServiceProvider } from './ajs-upgraded-providers';
import { languageServiceModule } from './ajs-upgraded-providers';
import { uuid2ServiceModule } from './ajs-upgraded-providers';

import { AppComponent } from './app.component';
import { BarModule } from './bar/bar.module';
import { NxLanguageDropdown } from "./dropdown/language.component";

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
        CoreModule,
        BarModule,
        uuid2ServiceModule,
        languageServiceModule,

        NgbModule.forRoot(),
        RouterModule.forRoot([], { initialNavigation: false })
    ],
    entryComponents: [
        NxLanguageDropdown
    ],
    providers: [
        { provide: LocationStrategy, useClass: PathLocationStrategy },
        { provide: UrlHandlingStrategy, useClass: HybridUrlHandlingStrategy },
        cloudApiServiceProvider
    ],
    declarations: [AppComponent],
    bootstrap: [AppComponent]
})

export class AppModule {
    ngDoBootstrap() {
    }
}

declare var angular: angular.IAngularStatic;
angular
        .module('cloudApp.directives')
        .directive('nxLanguageSelect', downgradeComponent({ component: NxLanguageDropdown }) as angular.IDirectiveFactory);



