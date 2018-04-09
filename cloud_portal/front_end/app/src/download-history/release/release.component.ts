import {
    Component,
    OnInit,
    Input,
    Inject
} from '@angular/core';

@Component({
    selector: 'nx-release',
    templateUrl: './src/download-history/release/release.component.html',
    // TODO: Load language tailored template
    // templateUrl: this.CONFIG.viewsDir + 'components/downloadRelease.html'
})
export class ReleaseComponent implements OnInit {
    @Input() release: any;

    constructor(@Inject('languageService') private language: any,
                @Inject('configService') private configService: any) {

    }

    ngOnInit(): void {
    }
}

