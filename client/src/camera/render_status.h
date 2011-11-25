#ifndef QN_RENDER_STATUS_H
#define QN_RENDER_STATUS_H

class QnRenderStatus {
public:
    enum RenderStatus {
        RENDERED_NEW_FRAME,     /**< New frame was rendered. */
        RENDERED_OLD_FRAME,     /**< No new frames available, old frame was rendered. */
        NOTHING_RENDERED,       /**< No frames to render, so nothing was rendered. */
        CANNOT_RENDER           /**< Something went wrong. */
    };
};

#endif // QN_RENDER_STATUS_H
