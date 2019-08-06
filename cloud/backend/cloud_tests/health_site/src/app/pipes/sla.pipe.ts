import { Pipe, PipeTransform } from '@angular/core';

@Pipe({name: 'sla', pure: false})
export class SlaPipe implements PipeTransform {

    constructor() {
    }

    transform(value: any, args?: any): any {
        return 100 - value.toFixed(4);
    }
}
