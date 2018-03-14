import { Component, OnInit, ChangeDetectorRef, Inject } from '@angular/core';
import { HttpErrorResponse } from "@angular/common/http";
import { HttpClient } from '@angular/common/http';

// import { cloudApiService } from "../scripts/services/cloud_api";
import { QuoteService } from '../../app/core/index';
// import { uuid2Service } from '../../app/scripts/services/angular-uuid2';
// import { languageService } from '../../app/scripts/services/language';

@Component({
    selector: 'bar-component',
    templateUrl: './bar/bar.component.html'
})

export class BarComponent implements OnInit {
    message: string;
    serviceMessage: string;
    languageMessage: any;
    title = 'bar';

    // cloudApi: any;
    // uuid2: any;
    // language: any;

    activeLanguage: any;

    constructor(@Inject('uuid2Service') private uuid2: any,
                @Inject('languageService') private language: any,
                @Inject('cloudApiService') private cloudApi: any,
                private http: HttpClient,
                private quoteService: QuoteService,
                private changeDetector: ChangeDetectorRef) {

        // this.uuid2 = uuid2;
        //this.language = language;
        //this.cloudApi = cloudApi;
    }

    // getLanguages(): any {
    //     return this.http.get('/static/languages.json', {});
    // }

    ngOnInit(): void {
        this.serviceMessage = this.uuid2.newguid();

        this.quoteService
                .getRandomQuote({ category: 'dev' })
                .subscribe(
                        (data: any) => {
                            this.message = data.value;
                            // this.changeDetector.detectChanges();
                        },
                        (err: HttpErrorResponse) => {
                            console.log(err.error);
                            console.log(err.name);
                            console.log(err.message);
                            console.log(err.status);
                        });

        // this.getLanguages()
        //         .subscribe((data: any) => {
        //             console.log(data);
        //             this.activeLanguage = data;
        //         });

        this.cloudApi
                .getLanguages()
                .then((data) => {
                    console.log('Data: ', data.data);
                    console.log('Length: ', data.data.length);
                });

    }
}

