# Watermarking Video & Images {#watermarks}

![](images/watermark.png)

## Feature Description 
Watermarks are used in NX Witness client to prevent unauthorized client screen filming. Technically watermarks are semi-transparent labels with NX client username that are shown above camera windows and saved layouts. If such images are filmed and published, it is possible to figure out who was responsible for the leak.

## Implementation Rules
- Watermark labels consist of the current user name shown above image and video windows. 
- Watermarks are set up by system administrator in System Administration->General->Security Settings dialog. An administrator may set up label density and opacity.
- Watermarks are not shown for system administrators, except for exported watermarked layouts.
- Watermarks are added to exported video and screenshots directly, by embedding them into frames  during transcoding.
- For exported layouts, watermark metadata is embedded into the Layout File (.nov or .exe), but video and image streams itself are kept intact.
- When playing an exported layout, embedded watermark is shown, even if played back by an admin.

## Technical Details
- Watermark content and properties are fully described by @ref nx::core::Watermark structure.
This structure consists of watermark text and @ref QnWatermarkSettings . Both structures are supported by Fusion for JSON serialization and equality operator. QVariant transformations are supported as well.
- It is important to deliver watermarks to images fast. Therefore watermarks are not typed on frames directly, first a watermark image of an appropriate size is created and then it is blended to the original image. This gives a considerable speedup when watermark is placed on an active camera or an exported video stream - watermark image is created once and reused. For consistency, drawing watermark on a single still image (screenshot) follows the same pattern.
- Watermark image is not necessary made of the same size as the underlying image, but it always has the same aspect ratio. Watermark images are cached, see "Caching Watermark Images" below.
- Core code to produce watermark images for blending is contained in @ref watermark_images.cpp . @ref nx::core::createWatermarkImage creates watermark image for a certain frame size. The cache is never used and returned image size is exactly as requested. @ref nx::core::retrieveWatermarkImage attempts to use cache and may return image with the same aspect ratio but different size (see below).
- @ref nx::vms::client::desktop::WatermarkPainter class is used to add watermark to an image. Provide it with a @ref nx::core::Watermark and it will be able to put watermark on any subsequent image that is passed to the `drawWatermark()`. This class uses watermark caching. One object of this class supports only one aspect ratio for images, recreate the object if you change it. 
- @ref QnMediaResourceWidget uses @ref nx::vms::client::desktop::WatermarkPainter to draw watermarks on the media windows, and @ref nx::vms::client::desktop::LayoutThumbnailLoader uses @ref nx::vms::client::desktop::WatermarkProxyProvider using @ref nx::vms::client::desktop::WatermarkPainter to draw watermarks for export preview and videowalls.
- Another branch using Watermark is for exporting video and screenshots. It is done via @ref nx::core::transcoding::WatermarkImageFilter that is utilized as normal image filter during transcoding process. 

## Watermark Image Caching 
- Watermark images are cached to blend them to video frames fast. 
- Since watermark images may be big (megabytes), it may be not convenient to keep an own image for every image or widget. Currently, one single global cache is used. 
- Watermark cache contains one image for every aspect ratio used. This is done to save memory, and also to avoid big images (size of media widget in scene coordinates can be something like 10000x6250). Image is added to cache if a watermark with a new aspect ratio is requested by @ref nx::core::retrieveWatermarkImage. 
- Watermark cache contains images for only one watermark text value. If request is made with different text, the entire cache is reset, dropping all existing images. This may happen when opening layout with a pre-saved watermark that is different from the one currently set. If you need special watermark text casually, use @ref nx::core::createWatermarkImage instead to bypass cache and prevent cache dropping.
- Image returned by cache follows aspect ratio, but may be of different size. Use scaled drawing if needed. The sizes are arbitrary and selected to make watermarks look reasonably good.
- Rules for image sizes that are put and returned from cache for different Aspect Ratios (AR):
  - `AR = 16:9 Size = 1920x1080`
  - `AR = 4:3  Size = 1600x1200`
  - `AR = 1:1  Size = 1200x1200`
  - Any other AR - minimal image dimension will be exactly 1200.

## Possible Architectural Limitations
- Current system may produce a distorted watermark if a camera changes its aspect ratio on the fly.






