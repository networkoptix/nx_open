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
      const query = filter.query;
      const queryTerms = query.trim().split(' ');
      const preferredVendors = '';

      function filterCamera(c, query) {
          function lowerNoDashes(str) {
              return str.replace(/-/g, '').toLowerCase();
          }

          const queryLowerNoDashes = lowerNoDashes(query);

          return (query.length === 0
              || lowerNoDashes(c.vendor).includes(queryLowerNoDashes)
              || lowerNoDashes(c.model).includes(queryLowerNoDashes)
              || c.maxResolution.includes(query));
      }

      const cameras = allCameras.filter(camera => {
          if (boolKeys.some(key => {
              return filter[key] === true && camera[key] !== true;
          })) {
              return false;
          }

          if (filter.minResolution.value !== 0 && camera.resolutionArea <= filter.minResolution.value * 0.9) {
              return false;
          }

          if (filter.vendors.length > 0 && filter.vendors.indexOf(camera.vendor) === -1) {
              return false;
          }

          if (filter.hardwareTypes.length > 0 && filter.hardwareTypes.indexOf(camera.hardwareType) === -1) {
              return false;
          }

          // Filter by query
          return queryTerms.every(term => {
              return filterCamera(camera, term);
          });
      }).sort(camera => {
          const key = (camera.vendor + camera.model).toLowerCase();

          if (preferredVendors.indexOf(camera.vendor.toLowerCase()) !== -1) {
              return '!' + key;
          }

          return key;
      });

      return of(cameras);
  }
}
