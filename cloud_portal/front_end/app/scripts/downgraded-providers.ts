import { downgradeInjectable } from '@angular/upgrade/static';
import { NgbModal } from '@ng-bootstrap/ng-bootstrap';

declare var angular: angular.IAngularStatic;

angular
        .module('cloudApp.services')
        .factory('NgbModal', downgradeInjectable(NgbModal) as any);
