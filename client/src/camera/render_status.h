#ifndef QN_RENDER_STATUS_H
#define QN_RENDER_STATUS_H

namespace Qn {

    enum RenderStatus {
        NewFrameRendered,   /**< New frame was rendered. */
        OldFrameRendered,   /**< No new frames available, old frame was rendered. */
        NothingRendered,    /**< No frames to render, so nothing was rendered. */
        CannotRender        /**< Something went wrong. */
    };

} // namespace Qn

#endif // QN_RENDER_STATUS_H
