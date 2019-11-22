/* AirScan (a.k.a. eSCL) backend for SANE
 *
 * Copyright (C) 2019 and up by Alexander Pevzner (pzz@apevzner.com)
 * See LICENSE for license terms and conditions
 *
 * JPEG image decoder
 */

#include "airscan.h"

#include <jpeglib.h>

/* JPEG image decoder
 */
typedef struct {
    image_decoder                 decoder;  /* Base class */
    struct jpeg_decompress_struct cinfo;    /* libjpeg decoder */
    struct jpeg_error_mgr         jerr;     /* libjpeg error manager */
    JDIMENSION                    num_rows; /* Num of rows left to read */
} image_decoder_jpeg;

/* Free JPEG decoder
 */
static void
image_decoder_jpeg_free (image_decoder *decoder)
{
    image_decoder_jpeg *jpeg = (image_decoder_jpeg*) decoder;

    jpeg_destroy_decompress(&jpeg->cinfo);
    g_free(jpeg);
}

/* Begin JPEG decoding
 */
static IMAGE_STATUS
image_decoder_jpeg_begin (image_decoder *decoder, const void *data,
        size_t size)
{
    image_decoder_jpeg *jpeg = (image_decoder_jpeg*) decoder;
    int                rc;

    jpeg_mem_src(&jpeg->cinfo, data, size);
    rc = jpeg_read_header(&jpeg->cinfo, true);
    if (rc != JPEG_HEADER_OK) {
        jpeg_abort((j_common_ptr) &jpeg->cinfo);
        return IMAGE_ERROR;
    }

    if (jpeg->cinfo.num_components != 1) {
        jpeg->cinfo.out_color_space = JCS_RGB;
    }

    jpeg_start_decompress(&jpeg->cinfo);
    jpeg->num_rows = jpeg->cinfo.image_height;

    return IMAGE_OK;
}

/* Reset JPEG decoder
 */
static void
image_decoder_jpeg_reset (image_decoder *decoder)
{
    image_decoder_jpeg *jpeg = (image_decoder_jpeg*) decoder;

    jpeg_abort((j_common_ptr) &jpeg->cinfo);
}

/* Get image parameters
 */
static void
image_decoder_jpeg_get_params (image_decoder *decoder, SANE_Parameters *params)
{
    image_decoder_jpeg *jpeg = (image_decoder_jpeg*) decoder;

    params->pixels_per_line = jpeg->cinfo.image_width;
    params->lines = jpeg->cinfo.image_height;
    params->depth = 8;

    if (jpeg->cinfo.num_components == 1) {
        params->format = SANE_FRAME_GRAY;
        params->bytes_per_line = params->pixels_per_line;
    } else {
        params->format = SANE_FRAME_RGB;
        params->bytes_per_line = params->pixels_per_line * 3;
    }
}

/* Set clipping window
 */
static void
image_decoder_jpeg_set_window (image_decoder *decoder, image_window *win)
{
    image_decoder_jpeg *jpeg = (image_decoder_jpeg*) decoder;
    JDIMENSION         x_off = win->x_off;
    JDIMENSION         wid = win->wid;

    jpeg_crop_scanline(&jpeg->cinfo, &x_off, &wid);
    if (win->y_off > 0) {
        jpeg_skip_scanlines(&jpeg->cinfo, win->y_off);
    }

    jpeg->num_rows = win->hei;

    win->x_off = x_off;
    win->wid = wid;
}

/* Read next row of image
 */
static IMAGE_STATUS
image_decoder_jpeg_read_row (image_decoder *decoder, void *buffer)
{
    image_decoder_jpeg *jpeg = (image_decoder_jpeg*) decoder;
    JSAMPROW           rows[1] = {buffer};

    if (!jpeg->num_rows) {
        return IMAGE_EOF;
    }

    if (jpeg_read_scanlines(&jpeg->cinfo, rows, 1) == 0) {
        return IMAGE_ERROR;
    }

    jpeg->num_rows --;

    return IMAGE_OK;
}

/* Create JPEG image decoder
 */
image_decoder*
image_decoder_jpeg_new (void)
{
    image_decoder_jpeg *jpeg = g_new0(image_decoder_jpeg, 1);

    jpeg->decoder.free = image_decoder_jpeg_free;
    jpeg->decoder.begin = image_decoder_jpeg_begin;
    jpeg->decoder.reset = image_decoder_jpeg_reset;
    jpeg->decoder.get_params = image_decoder_jpeg_get_params;
    jpeg->decoder.set_window = image_decoder_jpeg_set_window;
    jpeg->decoder.read_row = image_decoder_jpeg_read_row;

    jpeg->cinfo.err = jpeg_std_error(&jpeg->jerr);
    jpeg_create_decompress(&jpeg->cinfo);

    return &jpeg->decoder;
}

/* vim:ts=8:sw=4:et
 */
