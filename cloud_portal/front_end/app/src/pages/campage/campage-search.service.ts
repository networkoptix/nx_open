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

    campageSearch (allCameras, filter): Observable<any> {
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

      const resolution = filter.selects.find(x => x.id === 'resolution').selected;
      const vendors = filter.multiselects.find(x => x.id === 'vendors').selected;
      const types = filter.multiselects.find(x => x.id === 'hardwareTypes').selected;

      const cameras = allCameras.filter(camera => {
          if (filter.tags.some(key => {
              return key.value === true && camera[key.id] !== true;
          })) {
              return false;
          }

          if (resolution.value !== 0 && camera.resolutionArea <= resolution.value * 0.9) {
              return false;
          }

          if (vendors.length > 0 && vendors.indexOf(camera.vendor) === -1) {
              return false;
          }

          if (types.length > 0 && types.indexOf(camera.hardwareType) === -1) {
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
