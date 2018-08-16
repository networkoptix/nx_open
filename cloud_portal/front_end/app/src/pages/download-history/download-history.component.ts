import {
    Component, OnInit, OnDestroy, Input,
    ViewChild, Inject
}                                       from '@angular/core';
import { ActivatedRoute, Router }       from '@angular/router';
import { Title }                        from '@angular/platform-browser';
import { DOCUMENT, Location }           from '@angular/common';
import { NgbTabChangeEvent, NgbTabset } from '@ng-bootstrap/ng-bootstrap';

@Component({
    selector: 'download-history',
    templateUrl: 'download-history.component.html'
})

export class DownloadHistoryComponent implements OnInit, OnDestroy {
    @Input() routeParamBuild;
    @Input() section: string;

    private sub: any;
    private build: any;
    private deviceInfo = null;

    user: any;
    downloads: any;
    activeBuilds: any;
    downloadsData: any;
    downloadTypes: any;
    linkbase: any;

    location: Location;

    @ViewChild('tabs')
    public tabs: NgbTabset;

    constructor(@Inject('languageService') private language: any,
                @Inject('cloudApiService') private cloudApi: any,
                @Inject('configService') private configService: any,
                @Inject('authorizationCheckService') private authorizationService: any,
                @Inject('account') private account: any,
                @Inject(DOCUMENT) private document: any,
                private route: ActivatedRoute,
                private router: Router,
                private titleService: Title,
                location: Location) {

        this.location = location;
    }

    public beforeChange($event: NgbTabChangeEvent) {
        let section = $event.nextId;

        if (this.location.path() === '/downloads/' + section) {
            return;
        }

        // Tab is "releases" but the link is "history" ... a little bit schizophrenic
        section = (section !== 'releases') ? section : 'history';

        // TODO: Repace this once 'register' page is moved to A5
        // AJS and A5 routers freak out about route change *****
        // this.router.navigate(['/downloads/' + section]);
        this.document.location.href = '/downloads/' + section;
    };

    ngOnInit(): void {
        this.downloads = this.configService.config.downloads;

        this.sub = this.route.params.subscribe(params => {
            //this.build = params['build'];

            this.section = this.section || 'releases';

            this.build = this.routeParamBuild;

            this.authorizationService
                .requireLogin()
                .then(() => {
                    this.cloudApi
                        .getDownloadsHistory(this.build)
                        .then(result => {
                                this.linkbase = result.data.updatesPrefix;

                                if (!this.build) { // only one build
                                    this.activeBuilds = result.data.releases;
                                    this.downloadTypes = ['releases', 'patches', 'betas'];
                                    this.downloadsData = result.data;
                                } else {
                                    this.activeBuilds = [result.data];
                                    this.downloadTypes = [result.data.type];
                                    this.downloadsData = {};
                                    this.downloadsData[result.data.type] = this.activeBuilds;
                                }

                                this.titleService.setTitle(this.downloadTypes[0][0].toUpperCase() + this.downloadTypes[0].substr(1).toLowerCase());
                                this.activeBuilds = this.downloadsData[this.section];

                                setTimeout(() => {
                                    if (this.tabs) {
                                        this.tabs.select(this.section);
                                    }
                                });

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
}

