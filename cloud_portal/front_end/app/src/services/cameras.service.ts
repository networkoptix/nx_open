import { Injectable }   from '@angular/core';
import { HttpClient }   from '@angular/common/http';
import { Observable, of }   from 'rxjs';
import { catchError, tap } from 'rxjs/operators';

@Injectable({
  providedIn: 'root'
})
export class CamerasService {

    private cacamerasUrl = 'https://cameras.networkoptix.com/api/v1/cacameras/';

  constructor(private http: HttpClient) { }

  getAllCameras (company): Observable<any> {
    return this.http.get(this.cacamerasUrl,
        {
                    params: {
                        company
                    }
                });
       // .pipe(
        // tap(_ => this.log('fetched heroes')),
        // catchError(this.handleError('getAllCameras', []))
      // );
  }

  private handleError<T> (operation = 'operation', result?: T) {
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
