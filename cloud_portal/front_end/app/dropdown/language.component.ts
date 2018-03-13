import { Component, OnInit, ChangeDetectorRef } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { NgbDropdownModule } from '@ng-bootstrap/ng-bootstrap';

import { cloudApiService } from "../scripts/services/cloud_api";
import { languageService } from "../scripts/services/language";

export interface activeLanguage {
    language: string;
    name: string;
}

@Component({
    selector: 'nx-language-select',
    templateUrl: './dropdown/language.component.html',
    styleUrls: ['./dropdown/language.component.scss']
})

export class NxLanguageDropdown implements OnInit {

    languages: any;

    activeLanguage: any = {
        language : '',
        name: ''
    };

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

    constructor(private httpClient: HttpClient,
                private api: cloudApiService,
                private dropdown: NgbDropdownModule,
                private changeDetector: ChangeDetectorRef) {
    }


    getLanguages(): any {
        this.httpClient.get('/static/languages.json');
    }

    ngOnInit(): void {

        // this.getLanguages()
        //         .subscribe((data: any) => {
        //             this.activeLanguage = data.data;
        //         });

        console.log(this.activeLanguage);

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

        // this.changeLanguage();
    }
}

