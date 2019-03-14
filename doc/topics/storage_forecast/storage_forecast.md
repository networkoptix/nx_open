# Storage Forecast Component {#storage_forecast}

![](images/storage_forecast.png)

## Feature Description 
Sometimes NX servers have complex setup of cameras and their recording rules. Different cameras
could have different recording intervals and triggers, bitrates and required lifetime of video
recordings. It is important not only to have statistics on how much space is occupied at some given 
moment of time, but also to figure out when free space comes to an end and NX server will start
overwriting possibly valuable data. Another valuable thing is to know how adding extra space will
increase maximum time period of stored recordings.

Storage Forecast component is made to address these concerns. It allows NX client user to forecast
storage usage as well as play what if some additional storage is added to the system.
The component uses some special algorithms and assumptions to make these forecasts, they are being
discussed below. Forecast value is updated in the "Calendar days" column as the user changes
"Additional storage" value or "Base forecast on..." selector.

## Assumptions Made 
- Forecast is made based on previous camera performance during a specified period of time,
user-selectable from 5 minutes to all historical data.
- Only cameras that are currently recording or armed for recording are accounted for.
- Required space is based on recording space density - how much space camera recordings occupy
for the time period, not maximum or typical bitrate. For instance, if camera generally records
for five minutes per hour, it will be taken into account, and its recording density will be 1/12
of its typical bitrate.
- Available space is based on recorded volumes of all cameras that belong to the server, plus
any additional spare space that is available. If a camera has recordings, but do not participate
in the forecast, its recording space is also counted because it is assumed that the recordings
will be erased at some point.
- First priority is given to cameras that have "Min. days" set (not "Auto"). Next, if space is
enough, cameras that do not have "Min. days" can record too. Archive for a camera is limited to
"Max. days" if it is set.

## Algorithm to Calculate The Forecast
- First, all cameras that are recording or armed for recording are identified.
- For each of these cameras recording density (recording space divided by specified period of time)
is calculated.
- Total available space is calculated - recorded volumes of all cameras that belong to the server,
plus free server space, plus "Additional space" entered by user.
- Total space is divided between cameras in several phases, the process may stop and forecast value
is returned at any phase:
    - Phase 1: Only priority cameras (ones that have "Min. days" set) are recording simultaneously.
If the total space divided by cameras total recording bitrate is less than minimal of cameras "Min. 
days" parameter, it is returned as the time forecast value.
    - Phase 2: Total space is enough to hold all recordings of all priority cameras so that at
least one camera can record for more than its "Min. days" parameter. In this case it is "stopped"
at this point and remaining space is divided between other priority cameras. This is repeated until
either all total space is exhausted or all priority cameras have enough space to record up
to theirs "Min. days" parameter. If the space is exhausted, the corresponding time is returned as
the time forecast value.
    - Phase 3: Total space is enough to record all priority cameras for their "Min. days" parameter.
Then all remaining space is divided between all cameras (including non-priority) as if they were
recording simultaneously. If the remaining space divided by total recording density is less then
any cameras ("Max. days" - "Min. days") value ( "Max. days" set to "Auto" can be considered
as infinity, and "Min. days" set to "Auto" as 0), then maximum "Min. days" between all cameras plus
this value is returned as forecast.
    - Phase 4: Total space is enough to minimally record all priority cameras, plus remaining space
is enough to simultaneously record all cameras so that at least one of it reaches it "Max. days"
limit. Here this camera "stops" and all other space goes to remaining cameras. Then other cameras
with "Max. days" are "stopped". If space expires before all cameras can record up to their
"Max. days" limit, the period of simultaneous recording is added to maximum "Min. days" between
all cameras and this value is returned as forecast. This is always the case if at least one camera
has "Max. days" set to "Auto".
    - Phase 5: If total space is enough to record all cameras up to their "Max. days" period,
it is returned as the forecast.
 
## Example
Here is one example that partially comes from the Unit Test ( storage_forecaster_ut.cpp ).
Consider the following setup:

Camera   | Recording density (bytes/s) | Min. days | Max. days
---------|-----------------------------|-----------|----------
Camera 1 | 1000 | Auto | 1
Camera 2 | 2000 | Auto | 3
Camera 3 | 3000 | 1    | 2
Camera 4 | 4000 | 2    | 3
Camera 5 | 5000 | 4    | 7


### Case 1: TotalSpace = 2 * (3000 + 4000 + 5000)

Here we start distribute space for priority cameras 3,4,5. It is exhausted completely in two secs,
so we stop at Phase 1. The forecast for cameras in seconds is {0, 0, 2, 2, 2}.

### Case 2: TotalSpace = 86400 * (1 * 3000 + 2 * 4000 + 3 * 5000)

(86400 - number of seconds in a day)

Again, we start distribute space for priority cameras 3,4,5. The space is enough for all three cameras
to record for 1 day, so we only keep 1 day of recording for camera 3. The rest is enough to record
for cameras 4 and 5 for one more day, so we can keep 2 days of recordings for camera 4. However,
the rest is not enough to keep camera 5 recording for 2 more days, so it will record for 1 day only
and we stop at Phase 2. Complete forecast in days is {0, 0, 1, 2, 3}

### Case 2: TotalSpace = 86400 * (1 * 3000 + 2 * 4000 + 3 * 5000)

Again, we start distribute space for priority cameras 3,4,5. The space is enough for all three cameras
to record for 1 day, so we only keep 1 day of recording for camera 3. The rest is enough to record
for cameras 4 and 5 for one more day, so we can keep 2 days of recordings for camera 4. However,
the rest is not enough to keep camera 5 recording for 2 more days, so it will record for 1 day only
and we stop at Phase 2. Complete forecast in days is {0, 0, 1, 2, 3}

### Case 3: TotalSpace = 86400 * (1 * 1000 + 2 * 2000 + 2 * 3000 + 3 * 4000 + 6 * 5000)

The space is enough to keep records for all priority cameras for their "Min. days" value. Then
remaining space is divided between all cameras as if they were recording all together. It is enough
to record all of them for 1 day, so at this point we do not need any more space for camera 1, 3
and 4. This happens because camera all these cameras are recording for their "Max. days" period,
including previously reserved space for their "Min. days" records. The rest of space is divided
between cameras 2 and 5 alone, and it is enough to keep them going for one more day. So, in total,
we have "Max. days" of possible recording for cameras 1, 3 and 4. We also have 2 days of recording
for camera 2 and 6 ("Min. Days" + 2) days for camera 6. The forecast is {1, 2, 2, 3, 6} in days.

### Case 4: TotalSpace = 86400 * (1 * 1000 + 3 * 2000 + 2 * 3000 + 3 * 4000 + 7 * 5000 + 100000)

The space is enough to keep records for all cameras for their "Max. days" value. The forecast
in days is {1, 3, 2, 3, 7}.





