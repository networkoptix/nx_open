import { Injectable }             from '@angular/core';
import { TranslateService }       from '@ngx-translate/core';
import { ReplaySubject, Subject } from 'rxjs';

@Injectable({
    providedIn: 'root'
})
export class NxLanguageProviderService {

    lang: any;
    translations: any;
    // translationsSubject = new ReplaySubject();

    constructor(private translate: TranslateService) {
    }

    setDefaultLang(lang: string): void {
        this.translate.setDefaultLang(lang);
    }

    setTranslations(lang, json): void {
        this.translate.setTranslation(lang, json);
        this.translate.currentLang = lang;
    }

    getTranslations(): any {
        return this.translate.translations[this.translate.currentLang];
    }

    getLang(): string {
        return this.translate.currentLang;
    }


    ngOnDestroy(): void {
        // this.translationsSubject.unsubscribe();
    }
}
