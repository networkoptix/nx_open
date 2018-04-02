import {
    Component, OnInit, OnDestroy,
    AfterViewChecked, ViewChild, Inject
}                                       from '@angular/core';
import { ActivatedRoute, Router }       from '@angular/router';
import { Title }                        from "@angular/platform-browser";
import { DOCUMENT }                     from "@angular/common";
import { NgbTabChangeEvent, NgbTabset } from "@ng-bootstrap/ng-bootstrap";
import { DeviceDetectorService }        from 'ngx-device-detector';


@Component({
    selector: 'download-history-component',
    templateUrl: './download-history/download-history.component.html'
})

export class DownloadHistoryComponent implements OnInit, OnDestroy, AfterViewChecked {
    private sub: any;
    private build: any;
    private deviceInfo = null;

    downloads: any;
    activeBuilds: any;
    downloadsData: any;
    downloadTypes: any;
    linkbase: any;

    @ViewChild('tabs')
    public tabs: NgbTabset;

    constructor(@Inject('languageService') private language: any,
                @Inject('cloudApiService') private cloudApi: any,
                @Inject('configService') private configService: any,
                @Inject('authorizationCheckService') private authorizationService: any,
                @Inject(DOCUMENT) private document: any,
                private route: ActivatedRoute,
                private router: Router,
                private titleService: Title,
                private deviceService: DeviceDetectorService) {

    }

    public beforeChange($event: NgbTabChangeEvent) {
        const type = $event.nextId;

        this.activeBuilds = this.downloadsData[type]
    };

    ngOnInit(): void {
        this.downloads = this.configService.config.downloads;

        this.sub = this.route.params.subscribe(params => {
            this.build = params['build'];

            console.log(this.build);

            this.authorizationService.requireLogin().then(() => {
                this.cloudApi
                    .getDownloadsHistory(this.build)
                    .then(result => {
                            this.linkbase = result.data.updatesPrefix;

                            if (!this.build) { // only one build
                                this.activeBuilds = result.data.releases;
                                this.downloadTypes = ["releases", "patches", "betas"];
                                this.downloadsData = result.data;
                            } else {
                                this.activeBuilds = [result.data];
                                this.downloadTypes = [result.data.type];
                                this.downloadsData = {};
                                this.downloadsData[result.data.type] = this.activeBuilds;
                            }
                        }, error => {
                            this.router.navigate(['404']); // Can't find downloads.json in specific build
                        }
                    );
            });
        });

    }

    ngOnDestroy() {
        this.sub.unsubscribe();
    }

    ngAfterViewChecked(): void {
        // setTimeout(() => {
        //     if (this.tabs) {
        //         this.tabs.select(this.activeOs);
        //     }
        // });
    }
}

