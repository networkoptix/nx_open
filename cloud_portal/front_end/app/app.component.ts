import { Component, Inject } from '@angular/core';
import { TranslateService }  from "@ngx-translate/core";
import { CookieService }     from "ngx-cookie-service";

@Component({
    selector: 'nx-app',
    template: `
        <router-outlet></router-outlet>
        <div ng-view="" ng-model-options="{ updateOn: 'blur' }"></div>
    `,
})

export class AppComponent {

    constructor(private cookieService: CookieService,
                translate: TranslateService) {

        let langCookie = this.cookieService.get('language'),
            lang = langCookie || translate.getBrowserCultureLang().replace('-', '_');

        // this language will be used as a fallback when a translation
        // isn't found in the current language
        translate.setDefaultLang('en_US');

        // the lang to use, if the lang isn't available, it will use
        // the current loader to get them
        translate.use(lang);
    }
}
