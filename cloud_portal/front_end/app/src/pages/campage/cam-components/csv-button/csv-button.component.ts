import { Component } from '@angular/core';
import { Angular2CsvComponent } from 'angular2-csv';

// Component to customize the "export to csv" button

@Component({
  selector: 'nx-csv-button',
  templateUrl: './csv-button.component.html',
  styleUrls: ['./csv-button.component.scss']
})
export class CsvButtonComponent extends Angular2CsvComponent {}
