import { NgModule } from '@angular/core';
import { CommonModule } from '@angular/common';
import { BrowserModule } from '@angular/platform-browser';
import { UpgradeModule } from '@angular/upgrade/static';
import { RouterModule, Routes } from '@angular/router';

import { QuoteService } from "../../app/core";

import { BarComponent } from './bar.component';

const appRoutes: Routes = [
  { path: 'bar', component: BarComponent }
];

@NgModule({
  imports: [
    CommonModule,
    BrowserModule,
    UpgradeModule,
    RouterModule.forChild(appRoutes)
  ],
  providers: [
      QuoteService
  ],
  declarations: [ BarComponent ],
  bootstrap: []    
})
export class BarModule { }
