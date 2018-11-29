import { Injectable } from '@angular/core';
import { Observable, of } from 'rxjs';
import 'rxjs/add/operator/map';
import 'rxjs/add/operator/debounceTime';
import 'rxjs/add/operator/distinctUntilChanged';
import 'rxjs/add/operator/switchMap';

@Injectable({
  providedIn: 'root'
})
export class CampageSearchService {

  constructor() { }


    campageSearch (allCameras, filter, boolKeys): Observable<any> {
      var query = filter.query;

      var queryTerms = query.trim().split(' ');
      //var window.preferred_vendors = '';
      const preferred_vendors = '';


      function filterCamera(c, query) {
          function lowerNoDashes(str) {
              return str.replace(/-/g, '').toLowerCase();
          }

          var queryLowerNoDashes = lowerNoDashes(query);

          return (query.length == 0
              || lowerNoDashes(c.vendor).includes(queryLowerNoDashes)
              || lowerNoDashes(c.model).includes(queryLowerNoDashes)
              || c.maxResolution.includes(query));
      }

      var cameras = allCameras.filter(c => {
          if (boolKeys.some(key => {
              return filter[key] === true && c[key] !== true;
          })) {
              return false;
          }

          if (filter.minResolution.value != 0 && c.resolutionArea <= filter.minResolution.value * 0.9) {
              return false;
          }

          if (filter.vendors.length > 0 && filter.vendors.indexOf(c.vendor) === -1) {
              return false;
          }

          if (filter.hardwareTypes.length > 0 && filter.hardwareTypes.indexOf(c.hardwareType) === -1) {
              return false;
          }

          // Filter by query
          return queryTerms.every(term => {
              return filterCamera(c, term);
          });
      });

      var cameras = cameras.sort(c => {
          var key = (c.vendor + c.model).toLowerCase();

          if (preferred_vendors.indexOf(c.vendor.toLowerCase()) != -1) {
              return '!' + key;
          }

          return key;
      });

      return of(cameras);
  }
}
