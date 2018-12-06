import { Pipe, PipeTransform } from '@angular/core';

@Pipe({name: 'timeAgo', pure: false})
export class TimeAgoPipe implements PipeTransform {

    constructor() {
    }

    transform(value: any, args?: any): any {
        // Calculate item's age
        const t1: any = new Date(value);
        const t2: any = new Date(args);
        const age: any = Math.round((t2.getTime() - t1.getTime()) / 1000);

        let result: any = 'more than a day';

        if (age < 10) {
            result = 'few seconds ago';
        }  else if (age < 60) {
            result = Math.floor(age) + ' seconds ago';
        } else if (age < 3600) {
            result = Math.floor(age / 60) + ' minutes ago';
        } else if (age < 86400) {
            result = Math.floor((age / 60) / 60) + ' hours ago';
        }

        return '(' + result + ')';
    }
}
