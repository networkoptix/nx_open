import { Injectable }        from '@angular/core';
import { Observable, of }    from 'rxjs';
import { NxCloudApiService } from './nx-cloud-api';

@Injectable({
    providedIn: 'root'
})
export class CamerasService {

    constructor(private api: NxCloudApiService) {
    }

    getAllCameras(company): Observable<any> {
        return this.api.getAllCameras(company);
    }

    private handleError<T>(operation = 'operation', result?: T) {
        return (error: any): Observable<T> => {

            // TODO: send the error to remote logging infrastructure
            // console.error(error); // log to console instead

            // TODO: better job of transforming error for user consumption
            // this.log(`${operation} failed: ${error.message}`);

            // Let the app keep running by returning an empty result.
            return of(result as T);
        };
    }
}
