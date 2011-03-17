#ifndef _dxva_picture_h
#define _dxva_picture_h

/* Number of planes in a picture */
#define VOUT_MAX_PLANES                 5

#define PICTURE_PLANE_MAX (VOUT_MAX_PLANES)

/** Description of a planar graphic field */
typedef struct plane_t
{
    uint8_t *p_pixels;                        /**< Start of the plane's data */

    /* Variables used for fast memcpy operations */
    int i_lines;           /**< Number of lines, including margins */
    int i_pitch;           /**< Number of bytes in a line, including margins */

    /** Size of a macropixel, defaults to 1 */
    int i_pixel_pitch;

    /* Variables used for pictures with margins */
    int i_visible_lines;            /**< How many visible lines are there ? */
    int i_visible_pitch;            /**< How many visible pixels are there ? */

} plane_t;

struct picture_t
{
    /**
    * The properties of the picture
    */
    // video_frame_format_t format;

    void           *p_data_orig;                /**< pointer before memalign */
    plane_t         p[PICTURE_PLANE_MAX];     /**< description of the planes */
    int             i_planes;                /**< number of allocated planes */

    /** \name Picture management properties
    * These properties can be modified using the video output thread API,
    * but should never be written directly */
    /**@{*/
    unsigned        i_refcount;                  /**< link reference counter */
    // mtime_t         date;                                  /**< display date */
    bool            b_force;
    /**@}*/

    /** \name Picture dynamic properties
    * Those properties can be changed by the decoder
    * @{
    */
    bool            b_progressive;          /**< is it a progressive frame ? */
    bool            b_top_field_first;             /**< which field is first */
    unsigned int    i_nb_fields;                  /**< # of displayed fields */
    int8_t         *p_q;                           /**< quantification table */
    int             i_qstride;                    /**< quantification stride */
    int             i_qtype;                       /**< quantification style */
    /**@}*/

    /** Private data - the video output plugin might want to put stuff here to
    * keep track of the picture */
    // picture_sys_t * p_sys;

    /** This way the picture_Release can be overloaded */
    // void (*pf_release)( picture_t * );
    // picture_release_sys_t *p_release_sys;

    /** Next picture in a FIFO a pictures */
    // struct picture_t *p_next;
};

#endif // _dxva_picture_h
