import { Pipe, PipeTransform } from '@angular/core';
import { DatePipe }            from '@angular/common';

@Pipe({
    name: 'timeChecked'
})
export class TimeCheckedPipe extends DatePipe implements PipeTransform {

    transform(value: any, args?: any): any {
        const date = super.transform(value, 'MM/dd/yyyy HH:mm:ss');
        return 'Last checked ' + date;
    }

}
