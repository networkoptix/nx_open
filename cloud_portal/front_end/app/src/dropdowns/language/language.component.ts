import { Component, OnInit, Inject, ViewEncapsulation, Input } from '@angular/core';
import { TranslateService }                                    from "@ngx-translate/core";

export interface activeLanguage {
    language: string;
    name: string;
}

@Component({
    selector: 'nx-language-select',
    templateUrl: 'language.component.html',
    styleUrls: ['language.component.scss'],
    encapsulation: ViewEncapsulation.None
})

export class NxLanguageDropdown implements OnInit {
    @Input() dropup: any;
    @Input() short: any;

    show: boolean;
    direction: string;
    activeLanguage = {
        language: '',
        name: ''
    };
    languages = [];
    languages_col1 = [];
    languages_col2 = [];

    constructor(@Inject('cloudApiService') private cloudApi: any,
                @Inject('languageService') private language: any,
                private translate: TranslateService) {

        this.show = false;
    }

    private splitLanguages() {
        if (this.languages.length > 12) {
            const halfWayThough = Math.ceil(this.languages.length / 2);

            this.languages_col1 = this.languages.slice(0, halfWayThough);
            this.languages_col2 = this.languages.slice(halfWayThough, this.languages.length);
        }
    }

    changeLanguage(lang: string) {
        if (this.activeLanguage.language !== lang) {
            /*  TODO: Currently this is not needed because the language file will
            be loaded during page reload. Once we transfer everything to Angular 5
            we should use this for seamless change of language
            // this.translate.use(lang.replace('_', '-'));
            */

            this.cloudApi
                .changeLanguage(lang)
                .then(function () {
                    window.location.reload();
                    return false; // return false so event will not bubble to HREF
                });
        }

        return false; // return false so event will not bubble to HREF
    }

    ngOnInit(): void {
        this.direction = this.dropup ? 'dropup' : '';

        this.cloudApi
            .getLanguages()
            .then((data: any) => {
                this.languages = data.data;
                this.splitLanguages();

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
