#ifndef QN_RENDER_STATUS_H
#define QN_RENDER_STATUS_H

// TODO: #Elric move to client_globals.

namespace Qn {

    /**
     * Note that order is important.
     */
    enum RenderStatus {
        NothingRendered,    /**< No frames to render, so nothing was rendered. */
        CannotRender,       /**< Something went wrong. */
        OldFrameRendered,   /**< No new frames available, old frame was rendered. */
        NewFrameRendered    /**< New frame was rendered. */
    };

} // namespace Qn

#endif // QN_RENDER_STATUS_H
