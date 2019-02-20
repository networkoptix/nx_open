# Storage Forecast Component {#storage_forecast}

![](images/storage_forecast.png)

## Feature Description 
Sometimes NX servers have complex setup of cameras and their recording rules. Different cameras could have different recording intervals and triggers, bitrates and required lifetime of video recordings. It is important not only to have statistics on how much space is occupied at some given moment of time, but also to figure out when free space comes to an end. Another valuable thing is to know how adding extra space will increase maximum time period of stored recordings.

Storage Forecast component is made to address these concerns. It allows NX client user to forecast storage usage as well as play "what if" some additional storage is added to the system. The component uses some special algorithms and assumptions to make these forecasts, they are being discussed below. Forecast value is updated in the "Calendar days" column as the user changes "Additional storage" value or "Base forecast on..." selector.

## Assumptions Made 
- Forecast is made based on previous camera performance during a specified period of time, user-selectable from 5 minutes to all historical data. 
- Only cameras that are currently recording or armed for recording are accounted for.
- Required space is based on recording space density - how much space camera recordings occupy for the time period, not maximum or typical bitrate. For instance, if camera generally records for five minutes per hour, it will be taken into account, and its recording density will be 1/12 of its typical bitrate.
- Available space is based on recorded volumes of all cameras that belong to the server, plus any additional spare space that is available. If a camera has recordings, but do not participate in the forecast, its recording space is also counted because it is assumed that the recordings will be erased at some point.
- First priority is given to cameras that have "Min. days" set (not "Auto"). Next, if space is enough, cameras that do not have "Min. days" can record too. Archive for a camera is limited to "Max. days" if it is set.

## Algorithm to Calculate The Forecast
- First, all cameras that are recording or armed for recording are identified.
- For each of these cameras recording density (recording space divided by specified period of time) is calculated.
- Total available space is calculated - recorded volumes of all cameras that belong to the server, plus free server space, plus "Additional space" entered by user.
- Total space is divided between cameras in several phases, the process may stop and forecast value is returned at any phase:
    - Phase 1: Only priority cameras (ones that have "Min. days" set) are recording simultaneously. If the total space divided by cameras total recording bitrate is less than minimal of cameras "Min? days" parameter, it is returned as the time forecast value.
    - Phase 2: Total space is enough to hold all recordings of all priority cameras so that at least one camera can record for more than its "Min. days" parameter. In this case it is "stopped" at this point and remaining space is divided between other priority cameras. This is repeated until either all total space is exhausted or all priority cameras have enough space to record up to theirs "Min. days" parameter. If the space is exhausted, the corresponding time is returned as the time forecast value.
    - Phase 3: Total space is enough to record all priority cameras for their "Min. days" parameter. Then all remaining space is divided between all cameras (including non-priority) as if they were recording simultaneously. If the remaining space divided by total recording density is less then any cameras ("Max. days" - "Min. days") value ( "Max. days" set to "Auto" can be considered as infinity, and "Min. days" set to "Auto" as 0), then maximum "Min. days" between all cameras plus this value is returned as forecast.
    - Phase 4: Total space is enough to minimally record all priority cameras, plus remaining space is enough to simultaneously record all cameras so that at least one of it reaches it "Max. days" limit.
    Here this camera "stops" and all other space goes to remaining cameras. Then other cameras with "Max. days" are "stopped". If space expires before all cameras can record up to their "Max. days" limit, the period of simultaneous recording is added to maximum "Min. days" between all cameras and this value is returned as forecast. This is always the case if at least one camera has "Max. days" set to "Auto".
    - Phase 5: If total space is enough to record all cameras up to their "Max. days" period, it is returned as the forecast.
    
Some examples how this actually works could be found in the unit test for this module ( storage_forecaster_ut.cpp ).

