import { Component, OnInit, Inject, ViewEncapsulation } from '@angular/core';
import { TranslateService }                             from "@ngx-translate/core";

export interface activeLanguage {
    language: string;
    name: string;
}

@Component({
    selector: 'nx-language-select',
    templateUrl: 'language.component.html',
    styleUrls: ['language.component.scss'],
    encapsulation: ViewEncapsulation.None,
    inputs: ['accountMode'],
})

export class NxLanguageDropdown implements OnInit {
    accountMode: boolean;
    activeLanguage = {
        language: '',
        name: ''
    };
    languages = [];

    constructor(@Inject('cloudApiService') private cloudApi: any,
                @Inject('languageService') private language: any,
                private translate: TranslateService) {
    }

    changeLanguage(lang: string) {
        if (this.activeLanguage.language === lang) {
            return;
        }

        /*  TODO: Currently this is not needed because the language file will
            be loaded during page reload. Once we transfer everything to Angular 5
            we should use this for seamless change of language
            // this.translate.use(lang.replace('_', '-'));
        */

        this.cloudApi
            .changeLanguage(lang)
            .then(function () {
                window.location.reload();
            });
    }

    ngOnInit(): void {
        this.accountMode = this.accountMode || false;

        this.cloudApi
            .getLanguages()
            .then((data: any) => {
                this.languages = data.data;

                const browserLang = this.translate.getBrowserCultureLang().replace('-','_');

                this.activeLanguage = this.languages.find(lang => {
                    return (lang.language === (this.language.lang.language || browserLang));
                });

                if (!this.activeLanguage) {
                    this.activeLanguage = this.languages[0];
                }
            });
    }
}
