import { Inject, Injectable, PLATFORM_ID } from '@angular/core';
import { ActivatedRoute, Router }          from '@angular/router';
import { Observable }                      from 'rxjs';
import { Location }                        from '@angular/common';

interface Params {
    [key: string]: any;
}

@Injectable({
    providedIn: 'root'
})
export class NxUriService {
    private _pageOffset: number;

    constructor(private router: Router,
                private location: Location,
                private route: ActivatedRoute,
                @Inject(PLATFORM_ID) private platformId: object) {
    }

    set pageOffset(val) {
        this._pageOffset = val;
    }

    get pageOffset() {
        return this._pageOffset;
    }

    getURL() {
        return this.router.url.split('?')[0];
    }

    getURI(): Observable<any> {
        return this.route.queryParams;
    }

    updateURI(navigateTo?: string, queryParams: any = {}, replace?) {
        if (!navigateTo) {
            navigateTo = this.getURL();
        }
        replace = replace ? replace : false;
        // changes the route without moving from the current view
        return new Promise((resolve, reject) => {
            setTimeout(() => {
                return this.router.navigate([navigateTo], {
                    queryParams,
                    relativeTo: this.route,
                    replaceUrl: replace,
                    queryParamsHandling: 'merge'
                }).then(success => {
                    resolve(success);
                }, error => {
                    reject(error);
                });
            });
        });
    }

    resetURI(navigateTo: string, queryParams: any = {}) {
        this.router.navigate([navigateTo], {
            queryParams,
            relativeTo: this.route,
            replaceUrl: false,
        });
    }
}
