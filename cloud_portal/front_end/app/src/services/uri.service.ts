import { Inject, Injectable, PLATFORM_ID }       from '@angular/core';
import { ActivatedRoute, Router, NavigationEnd } from '@angular/router';
import { Observable }                            from 'rxjs';
import { isPlatformBrowser, Location }           from '@angular/common';


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

    getURI(): Observable<any> {
        return this.route.queryParams;
    }

    updateURI(navigateTo: string, queryParams: any = {}) {
        // changes the route without moving from the current view
        this.router.navigate([navigateTo], {
            queryParams,
            relativeTo         : this.route,
            replaceUrl         : false,
            queryParamsHandling: 'merge',
            // do not trigger navigation
            // skipLocationChange : true
        });
    }

    resetURI(navigateTo: string) {
        this.router.navigate([navigateTo], {
            queryParams: {}
        });
    }
}
