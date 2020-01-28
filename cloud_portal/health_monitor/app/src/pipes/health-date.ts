import { formatDate } from '@angular/common';
import { Inject, LOCALE_ID, Pipe, PipeTransform } from '@angular/core';

@Pipe({ name: 'NxHealthDate'})
export class NxHealthDatePipe implements PipeTransform {
    constructor(@Inject(LOCALE_ID) private locale: string) {
    }

    transform(date: string|number, format?: string) {
        if (date === 'now') {
            return date;
        }

        if (!format) {
            format = 'MM/dd/yyyy, HH:mm';
        }
        return formatDate(new Date(date), format, this.locale);
    }
}
