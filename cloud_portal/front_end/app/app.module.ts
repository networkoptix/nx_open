import { NgModule } from '@angular/core';
import { PathLocationStrategy, LocationStrategy, CommonModule } from '@angular/common';
import { BrowserModule } from '@angular/platform-browser';
import { UpgradeModule } from '@angular/upgrade/static';
import { RouterModule, UrlHandlingStrategy, UrlTree, Routes } from '@angular/router';
import { HttpClientModule, HttpClient } from '@angular/common/http';

import { CoreModule } from './core/index';

import { AppComponent } from './app.component';
import { BarModule } from './bar/bar.module';

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

        RouterModule.forRoot([], { initialNavigation: false })
    ],
    providers: [
        { provide: LocationStrategy, useClass: PathLocationStrategy },
        { provide: UrlHandlingStrategy, useClass: HybridUrlHandlingStrategy }
    ],
    declarations: [AppComponent],
    bootstrap: [AppComponent]
})

export class AppModule {
    ngDoBootstrap() {
    }
}
