#include "jpeg.h"

#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
#define USE_LIBJPEG
#endif

#ifdef USE_LIBJPEG
#include <libjpeg/jpeglib.h>
#include <setjmp.h>
#include <QtCore/QDebug>
#endif

#ifdef USE_LIBJPEG

namespace
{

    void imageCleanup(void *info)
    {
        delete [](uchar*)info;
    }

    struct error_mgr : jpeg_error_mgr
    {
        jmp_buf setjmp_buffer;
    };

    void error_exit(j_common_ptr cinfo)
    {
        error_mgr *err = static_cast<error_mgr *>(cinfo->err);

        char message[JMSG_LENGTH_MAX];
        (*cinfo->err->format_message)(cinfo, message);
        qWarning() << message;

        longjmp(err->setjmp_buffer, 1);
    }

}

QImage decompressJpegImage(const char *data, size_t size)
{
    jpeg_decompress_struct jinfo;
    error_mgr jerr;
    uchar *buffer = nullptr;

    jinfo.err = jpeg_std_error(&jerr);
    jerr.error_exit = &error_exit;

    if (setjmp(jerr.setjmp_buffer))
    {
        jpeg_destroy_decompress(&jinfo);
        if (buffer)
            delete []buffer;
        return QImage();
    }

    jpeg_create_decompress(&jinfo);
    jpeg_mem_src(&jinfo, (uchar*)data, size);
    jpeg_read_header(&jinfo, TRUE);
    jinfo.out_color_space = JCS_EXT_BGRX;

    jpeg_start_decompress(&jinfo);

    int width = jinfo.output_width;
    int height = jinfo.output_height;
    int bytesPerLine = jinfo.output_width * jinfo.output_components;
    buffer = new uchar[bytesPerLine * height];

    while (jinfo.output_scanline < jinfo.output_height)
    {
        uchar *line = buffer + bytesPerLine * jinfo.output_scanline;
        jpeg_read_scanlines(&jinfo, &line, 1);
    }

    jpeg_finish_decompress(&jinfo);
    jpeg_destroy_decompress(&jinfo);

    return QImage(buffer, width, height, QImage::Format_ARGB32, &imageCleanup, buffer);
}
#else // USE_LIBJPEG
QImage decompressJpegImage(const char *data, size_t size)
{
    return QImage::fromData(reinterpret_cast<const uchar*>(data), (int) size);
}
#endif // USE_LIBJPEG

QImage decompressJpegImage(const QByteArray &data)
{
    return decompressJpegImage(data.data(), data.size());
}
