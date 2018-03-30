import { TestBed }           from '@angular/core/testing';
import { DownloadComponent } from './download.component';

describe('App', () => {
    
  beforeEach(() => {
    TestBed.configureTestingModule({ 
      declarations: [DownloadComponent],
      providers: [{ provide: 'barService', useValue: { getData: () => {} } }]
    });
  });

  it ('should work', () => {
    let fixture = TestBed.createComponent(DownloadComponent);
    expect(fixture.componentInstance instanceof DownloadComponent).toBe(true, 'should create DownloadComponent');
  });
});
