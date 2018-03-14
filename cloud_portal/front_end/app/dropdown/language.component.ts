import { Component, OnInit, ChangeDetectorRef, Inject } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { NgbDropdownModule } from '@ng-bootstrap/ng-bootstrap';

export interface activeLanguage {
    language: string;
    name: string;
}

@Component({
    selector: 'nx-language-select',
    templateUrl: './dropdown/language.component.html',
    styleUrls: ['./dropdown/language.component.scss'],
    inputs: ['accountMode'],
})

export class NxLanguageDropdown implements OnInit {
    accountMode: boolean;
    activeLanguage = {
        language: '',
        name: ''
    };
    languages = [];

    // function (language) {
    //     if (!this.accountMode) {
    //         if (language == L.language) {
    //             return;
    //         }
    //         this.api.get().changeLanguage(language).then(function () {
    //             window.location.reload();
    //         });
    //     } else {
    //         this.ngModel = language;
    //         this.activeLanguage = _.find(this.languages, function (lang) {
    //             return lang.language == this.ngModel;
    //         });
    //     }
    // }

    constructor(@Inject('cloudApiService') private cloudApi: any,
                @Inject('languageService') private language: any,
                private httpClient: HttpClient,
                private dropdown: NgbDropdownModule,
                private changeDetector: ChangeDetectorRef) {
    }

    changeLanguage(lang: activeLanguage){
        this.activeLanguage = lang;
    }

    ngOnInit(): void {

        // this.getLanguages()
        //         .subscribe((data: any) => {
        //             this.activeLanguage = data.data;
        //         });

        // console.log(this.activeLanguage);

        // this.api.get().getLanguages()
        //         .subscribe((data: any) => {
        //             let languages = data.data;
        //
        //             console.log(data);
        //
        //             this.activeLanguage = languages.find(lang => {
        //                 return (lang.language === languageService.get().lang.language);
        //             });
        //
        //             if (!this.activeLanguage) {
        //                 this.activeLanguage = languages[0];
        //             }
        //             this.languages = languages;
        //         });

        this.accountMode = this.accountMode || false;

        this.cloudApi
                .getLanguages()
                .then((data: any) => {
                    this.languages = data.data;

                    this.activeLanguage = this.languages.find(lang => {
                        return (lang.language === this.language.lang.language);
                    });

                    if (!this.activeLanguage) {
                        this.activeLanguage = this.languages[0];
                    }
                });
    }
}

// angular
//         .module('cloudApp.directives')
//         .directive('nxLanguageSelect', downgradeComponent({ component: NxLanguageDropdown }) as angular.IDirectiveFactory);
//
//
