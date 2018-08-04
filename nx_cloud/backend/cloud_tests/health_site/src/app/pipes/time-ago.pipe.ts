import { Pipe, PipeTransform } from '@angular/core';

@Pipe({name: 'timeAgo', pure: false})
export class TimeAgoPipe implements PipeTransform {

    constructor() {
    }

    transform(value: any, args?: any): any {
        // Calculate item's age
        let t1 = new Date(value).getTime();
        let t2 = new Date(args).getTime();
        let age: any = (t2 - t1) / 1000;

        if (age < 10) {
            age = 'few seconds ago';
        }  else if (age < 60) {
            age = Math.ceil(age) + ' seconds ago';
        } else if (age < 3600) {
            age = Math.ceil(age / 60) + ' minutes ago';
        } else if (age < 86400) {
            age = Math.ceil((age / 60) / 24) + ' hours ago';
        }

        return '(' + age + ')';
    }
}
