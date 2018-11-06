import { Component, Input, OnInit, ViewEncapsulation } from '@angular/core';
import { DomSanitizer } from '@angular/platform-browser';
import { NxConfigService } from '../../services/nx-config';

@Component({
    selector: 'nx-external-video',
    templateUrl: 'external-video.component.html',
    encapsulation: ViewEncapsulation.None,
    styleUrls: [ 'external-video.component.scss' ]
})
export class NxExternalVideoComponent implements OnInit {
    @Input('src') videoSrc: string;
    src: any;

    constructor(private sanitizer: DomSanitizer,
                private config: NxConfigService) {}

    private FormatSrc(link){
        const youtube = link.match(this.config.config.youtubeRegex);
        if (youtube && youtube[1]) {
            return `${this.config.config.youtubeEmbed}${youtube[1]}`;
        }

        const vimeo = link.match(this.config.config.vimeoRegex);
        if (vimeo && vimeo[1]) {
            return `${this.config.config.vimeoEmbed}${vimeo[1]}`;
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
