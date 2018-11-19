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
        for (let videoType in this.config.config.embedInfo) {
            const videoRegex = link.match(this.config.config.embedInfo[videoType].regex);
            if (videoRegex && videoRegex[1]) {
                return `${this.config.config.embedInfo[videoType].link}${videoRegex[1]}`;
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
