import { Component, Input, OnInit } from '@angular/core';
import { DomSanitizer } from '@angular/platform-browser';
import { NxConfigService } from '../../services/nx-config';

@Component({
    selector: 'nx-external-video',
    templateUrl: 'external-video.component.html',
    styleUrls: [ 'external-video.component.scss' ]
})
export class NxExternalVideoComponent implements OnInit {
    @Input('src') videoSrc: string;
    src: any;

    constructor(private sanitizer: DomSanitizer,
                private config: NxConfigService) {}

    private FormatSrc(link){
        for (let i in this.config.config.embedRegex) {
            const videoRegex = link.match(this.config.config.embedRegex[i]);
            if (videoRegex && videoRegex[1]) {
                return `${this.config.config.embedLinks[i]}${videoRegex[1]}`;
            }
        }
        return undefined;
    }

    ngOnInit() {
        this.src = this.sanitizeLink(this.FormatSrc(this.videoSrc));
    }

    sanitizeLink(link){
        return this.sanitizer.bypassSecurityTrustResourceUrl(link);
    }
}
