import { Injectable }          from '@angular/core';
import { downgradeInjectable } from '@angular/upgrade/static';
import { TranslateService }    from '@ngx-translate/core';

@Injectable({
    providedIn: 'root'
})
export class NxLanguageProviderService {

    lang: any;

    constructor(private translate: TranslateService) {
        this.lang = this.translate.getDefaultLang();
    }

    setLang(lang) {
        this.lang = lang;
        this.translate.use(lang);
    }

    getLang() {
        return this.lang;
    }
}
