import { Injectable }                      from '@angular/core';
import { downgradeInjectable }             from '@angular/upgrade/static';
import { TranslateService }                from '@ngx-translate/core';
import { BehaviorSubject, Observable, of } from 'rxjs';

@Injectable({
    providedIn: 'root'
})
export class NxLanguageProviderService {

    lang: any;
    translationsSubject = new BehaviorSubject({});

    constructor(private translate: TranslateService) {
        this.lang = this.translate.getDefaultLang();

        this.getTranslations().subscribe(lang => {
            this.translationsSubject.next(lang);
        });
    }

    private getTranslations(): Observable<any> {
        if (Object.keys(this.translate.translations).length) {
            return of(this.translate.translations[this.translate.currentLang]);
        }
        return of(this.translate.translations);
    }

    setLang(lang) {
        this.lang = lang;
        this.translate.use(lang);

        this.getTranslations().subscribe(lang => {
            this.translationsSubject.next(lang);
        });
    }

    getLang() {
        return this.lang;
    }


    ngOnDestroy() {
        this.translationsSubject.unsubscribe();
    }
}
