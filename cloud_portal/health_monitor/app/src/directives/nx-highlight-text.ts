import { PipeTransform, Pipe } from '@angular/core';

@Pipe({ name: 'highlight' })
export class HighlightPipe implements PipeTransform {
    transform(text: string, search): string {
        if (!search) {
            return text;
        }

        let pattern = search.replace(/[\-\[\]\/\{\}\(\)\*\+\?\.\\\^\$\|]/g, '\\$&');
        pattern = pattern.split(',').filter((t) => {
            return t.length > 0;
        }).join('|');

        const regex = new RegExp(pattern, 'gi');

        return text.replace(regex, (match) => `<span class="highlighted">${match}</span>`);
    }
}