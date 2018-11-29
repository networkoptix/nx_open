import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { CsvButtonComponent } from './csv-button.component';

describe('CsvButtonComponent', () => {
  let component: CsvButtonComponent;
  let fixture: ComponentFixture<CsvButtonComponent>;

  beforeEach(async(() => {
    TestBed.configureTestingModule({
      declarations: [ CsvButtonComponent ]
    })
    .compileComponents();
  }));

  beforeEach(() => {
    fixture = TestBed.createComponent(CsvButtonComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
