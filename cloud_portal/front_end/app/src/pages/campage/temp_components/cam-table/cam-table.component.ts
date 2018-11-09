import {Component, Input, OnInit} from '@angular/core';

@Component({
  selector: 'nx-cam-table',
  templateUrl: './cam-table.component.html',
  styleUrls: ['./cam-table.component.scss']
})
export class CamTableComponent implements OnInit {

  @Input() headers: string[];
  @Input() elements: any[];

  constructor() { }

  ngOnInit() {
  }

}
