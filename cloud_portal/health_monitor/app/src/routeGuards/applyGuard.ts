import { Injectable } from '@angular/core';
import { ActivatedRouteSnapshot, CanActivate, CanDeactivate, RouterStateSnapshot, UrlTree } from '@angular/router';
import { Observable } from 'rxjs';
import { NxApplyService } from '../services/apply.service';


@Injectable()
export class ApplyGuard implements CanActivate, CanDeactivate<any> {
    constructor(private applyService: NxApplyService) {}

    canActivate(route: ActivatedRouteSnapshot,
                state: RouterStateSnapshot
    ): Observable<boolean | UrlTree> | Promise<boolean | UrlTree> | boolean | UrlTree {
        return this.applyService.canMove().then((allowed) => {
            return allowed;
        });
    }

    canDeactivate(component: any,
                  currentRoute: ActivatedRouteSnapshot,
                  currentState: RouterStateSnapshot,
                  nextState?: RouterStateSnapshot
    ): Observable<boolean | UrlTree> | Promise<boolean | UrlTree> | boolean | UrlTree {
        return this.applyService.canMove().then((allowed) => {
            return allowed;
        });
    }
}
