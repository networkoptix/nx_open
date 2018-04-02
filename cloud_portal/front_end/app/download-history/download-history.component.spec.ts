import { TestBed }                  from '@angular/core/testing';
import { DownloadHistoryComponent } from './download-history.component';

describe('App', () => {
    
  beforeEach(() => {
    TestBed.configureTestingModule({ 
      declarations: [DownloadHistoryComponent],
      providers: [{ provide: 'barService', useValue: { getData: () => {} } }]
    });
  });

  it ('should work', () => {
    let fixture = TestBed.createComponent(DownloadHistoryComponent);
    expect(fixture.componentInstance instanceof DownloadHistoryComponent).toBe(true, 'should create DownloadHistoryComponent');
  });
});
