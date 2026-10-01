/*****************************************************************/ /**
 * \file   aimage.cpp
 * \brief  Image processing (based on libjpeg-turbo)
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "aimage.h"
#include <csetjmp>
#include <jpeglib.h>

using namespace alx;

// libjpeg's default error_exit calls exit(), which would take the host process with it; this one
// unwinds to the setjmp at each call site instead, so malformed input becomes a null return
namespace {
    struct jpeg_escape {
        struct jpeg_error_mgr pub;
        jmp_buf slot;
    };

    void jpeg_escape_exit(j_common_ptr _cinfo) {
        longjmp(((jpeg_escape*) _cinfo->err)->slot, 1);
    }
}

// BMP on-disk headers, read and written as raw bytes: member order and sizeof are the file format
namespace bmp_struct {
    struct mesg_head {
        mesg_head(uint_32 _w, uint_32 _h, uint_8 _depth) : w(_w), h(_h), depth(_depth), dsize(uint_32(image_base::align_line_bytes(_w, _depth) * _h)) {}
        uint_32 line_bytes() const { return dsize / h; }
        uint_32 size{sizeof(mesg_head)};
        uint_32 w{0};
        uint_32 h{0};
        uint_16 planes{1};
        uint_16 depth{0};
        uint_32 cmps{0};
        uint_32 dsize{0};
        uint_32 hdpi{0};
        uint_32 vdpi{0};
        uint_32 useclr{0};
        uint_32 impclr{0};
    };
    struct file_head {
        file_head(const mesg_head& _mesg) : size(2 + sizeof(file_head) + sizeof(mesg_head) + _mesg.dsize) {};
        uint_32 size{0};
        uint_16 rd1{0};
        uint_16 rd2{0};
        uint_32 ofst{2 + sizeof(file_head) + sizeof(mesg_head)};
    };
    constexpr uint_64 min_file_size = sizeof(mesg_head) + sizeof(file_head) + 2;
}

image_base* alx::image_base::create_image(uint_32 _w, uint_32 _h, uint_8 _dp, uint_64 _lb) {
    // divided rather than multiplied: an extreme declared w x h would wrap the product
    constexpr uint_64 max_image_bytes = 0X40000000ULL;

    if (0 != _h && alx::max_value(align_line_bytes(_w, _dp), _lb) > max_image_bytes / _h) return nullptr;
    switch (_dp) {
    case 1: return new image<1>(_w, _h, _lb);
    case 4: return new image<4>(_w, _h, _lb);
    case 8: return new image<8>(_w, _h, _lb);
    case 16: return new image<16>(_w, _h, _lb);
    case 24: return new image<24>(_w, _h, _lb);
    case 32: return new image<32>(_w, _h, _lb);
    default: return nullptr;
    }
}

image_base* alx::image_base::load_image_bmp(const bytes_view& _data) {
    if (_data.size() < bmp_struct::min_file_size) return nullptr;
    const bmp_struct::file_head head = m_interpret<bmp_struct::file_head>(_data.data() + 2);
    const bmp_struct::mesg_head mesg = m_interpret<bmp_struct::mesg_head>(_data.data() + sizeof(bmp_struct::file_head) + 2);

    // a zero height is refused here: the source stride below divides by it
    if (mesg.w == 0 || mesg.h == 0) return nullptr;
    const uint_64 ofst = head.ofst;
    if (ofst > _data.size()) return nullptr;
    if (mesg.useclr != 0 && ((uint_64) mesg.useclr << 2) != ofst - bmp_struct::min_file_size) return nullptr;
    if (mesg.dsize != 0 && ofst + mesg.dsize != _data.size()) return nullptr;
    image_base* result = create_image(mesg.w, mesg.h, (uint_8) mesg.depth);
    if (nullptr == result) return result;
    if (mesg.useclr != 0) {
        std::vector<bgra32> colors;
        for (uint_32 i = 0; i < mesg.useclr; i++)
            colors.push_back(m_interpret<bgra32>(_data.data() + bmp_struct::min_file_size + ((uint_64) i << 2)));
        result->set_palette(colors);
    }

    uint_8* dst_begin = result->data();
    uint_8* dst = result->data() + result->byte_count();
    const uint_8* src = _data.data() + ofst;
    const uint_8* src_end = _data.data() + (mesg.dsize != 0 ? ofst + mesg.dsize : _data.size());

    const uint_64 cpy = result->usfu_line_bytes();
    const uint_64 dsp = result->line_bytes();
    // source stride from the span rather than the width, so a file that declares no dsize still walks
    const uint_64 ssp = (uint_64) (src_end - src) / mesg.h;

    // BMP rows are bottom-up, so dst steps back from the buffer end while src steps forward
    // each step copies the tight row only, leaving the destination's padding bytes untouched
    while (dst_begin < dst && src < src_end) {
        dst -= dsp;
        memcpy(dst, src, alx::min_value<uint_64>(cpy, (uint_64) (src_end - src)));
        src += ssp;
    }

    return result;
}

image_base* alx::image_base::load_image_jpg(const bytes_view& _data) {
    if (_data.empty()) return nullptr;
    struct jpeg_decompress_struct cinfo;
    struct jpeg_escape jerr;
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jpeg_escape_exit;

    image_base* img{nullptr};
    if (setjmp(jerr.slot)) {
        jpeg_destroy_decompress(&cinfo);
        return nullptr;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, _data.data(), (uint_32) _data.size());

    if (JPEG_HEADER_OK == jpeg_read_header(&cinfo, TRUE)) {
        // EXT_ colour spaces hand back bytes in pixel<24>/<32> order, so a row decodes straight into the image
        cinfo.out_color_space = 1 == cinfo.num_components ? JCS_GRAYSCALE : 3 == cinfo.num_components ? JCS_EXT_BGR
                                                                                                      : JCS_EXT_BGRA;
        if (jpeg_start_decompress(&cinfo)) {
            JSAMPROW row_pointer{nullptr};
            if (0 != cinfo.output_width && 0 != cinfo.output_height) {
                img = image_base::create_image(cinfo.output_width, cinfo.output_height,
                                               1 == cinfo.num_components ? 8 : 3 == cinfo.num_components ? 24
                                                                                                         : 32);
                if (nullptr != img)
                    while (cinfo.output_scanline < cinfo.output_height) {
                        row_pointer = (JSAMPLE*) (img->data() + cinfo.output_scanline * img->line_bytes());
                        JDIMENSION lines_read = jpeg_read_scanlines(&cinfo, &row_pointer, 1);
                        if (lines_read != 1) {
                            delete img;
                            img = nullptr;
                            break;
                        }
                    }
            }
            jpeg_finish_decompress(&cinfo);
        }
    }
    jpeg_destroy_decompress(&cinfo);

    return img;
}

#ifdef _WIN32
image_base* alx::image_base::load_image_hbm(HBITMAP _hbm, DWORD _bcmps) {
    if (nullptr == _hbm) return nullptr;
    BITMAP bmp;
    GetObjectW(_hbm, sizeof(BITMAP), &bmp);
    if (bmp.bmWidth <= 0 || bmp.bmHeight <= 0 || bmp.bmBitsPixel == 0 || bmp.bmWidthBytes == 0) return nullptr;

    image_base* result = image_base::create_image(bmp.bmWidth, bmp.bmHeight, (uint_8) bmp.bmBitsPixel, (uint_64) bmp.bmWidthBytes);
    if (nullptr == result || !result->valid()) return delete result, nullptr;

    BITMAPINFO bmi = {0};
    memset(&bmi, 0, sizeof(BITMAPINFO));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = bmp.bmWidth;
    // a negative height asks GetDIBits for top-down rows, the order the image stores them in
    bmi.bmiHeader.biHeight = -bmp.bmHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = bmp.bmBitsPixel;
    bmi.bmiHeader.biCompression = _bcmps;

    HDC hdc = GetDC(nullptr);
    if (bmp.bmHeight != GetDIBits(hdc, _hbm, 0, bmp.bmHeight, result->data(), &bmi, DIB_RGB_COLORS)) {
        delete result;
        result = nullptr;
    }
    ReleaseDC(nullptr, hdc);

    // rows arrived tightly packed: spreading them apart has to go backwards or a row is clobbered
    if (nullptr != result && bmp.bmWidthBytes < result->line_bytes()) {
        uint_8* begin = result->data();
        uint_8* dst = result->data() + result->byte_count();
        const uint_8* src = result->data() + (uint_64) bmp.bmWidthBytes * (uint_64) bmp.bmHeight;
        while (dst > begin) {
            dst -= result->line_bytes();
            src -= bmp.bmWidthBytes;
            memcpy(dst, src, bmp.bmWidthBytes);
        }
    }

    return result;
}
#endif

bytes alx::image_base::to_bmp() const {
    if (!valid() || 0 == w || 0 == h) return bytes();

    bmp_struct::mesg_head mesg(this->w, this->h, depth());
    bmp_struct::file_head head(mesg);
    bytes result("BM");
    const std::vector<bgra32>* plt = palette();
    if (plt != nullptr && plt->size() != 0 && mesg.depth <= 8) {
        mesg.useclr = (uint_32) plt->size();
        head.size += (uint_32) (plt->size() << 2);
        head.ofst += (uint_32) (plt->size() << 2);
        result.append_ordinary(head);
        result.append_ordinary(mesg);
        result.append(bytes(plt->data(), plt->size() * 4));
    } else {
        result.append_ordinary(head);
        result.append_ordinary(mesg);
    }
    const uint_64 img_line_bytes = this->line_bytes();
    const uint_64 cpy_line_bytes = this->usfu_line_bytes();
    // mesg_head::line_bytes() is the aligned BMP row, dsize / h; what it exceeds the tight row by is padding
    const bytes padding(mesg.line_bytes() - cpy_line_bytes, 0);
    for (int row = this->h - 1; row >= 0; row--) {
        result.append(this->d.mid(row * img_line_bytes, cpy_line_bytes));
        result.append(padding);
    }
    return result;
}

bytes alx::image_base::to_jpg(uint_8 _quality) const {
    if (!valid() || 0 == w || 0 == h || (depth() != 8 && depth() != 24 && depth() != 32)) return bytes();
    jpeg_compress_struct cinfo;
    jpeg_escape jerr;
    JSAMPROW row_pointer;
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jpeg_escape_exit;
    unsigned char* out_buffer{nullptr};
    unsigned long out_size{0};
    bytes result;
    const uint_8 channel = depth() >> 3;

    if (setjmp(jerr.slot)) {
        jpeg_destroy_compress(&cinfo);
        free(out_buffer);
        return bytes();
    }

    jpeg_create_compress(&cinfo);
    jpeg_mem_dest(&cinfo, &out_buffer, &out_size);

    cinfo.image_width = this->w;
    cinfo.image_height = this->h;
    cinfo.input_components = channel;

    switch (channel) {
    case 1: cinfo.in_color_space = JCS_GRAYSCALE; break;
    case 3: cinfo.in_color_space = JCS_EXT_BGR; break;
    case 4: cinfo.in_color_space = JCS_EXT_BGRA; break;
    default: goto end;
    }

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, _quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer = (JSAMPROW) & this->d.data()[cinfo.next_scanline * line_bytes()];
        jpeg_write_scanlines(&cinfo, &row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);

    if (nullptr != out_buffer && out_size > 0) {
        result.resize(out_size);
        memcpy(result.data(), out_buffer, out_size);
    } else goto end;

    ;
end:
    jpeg_destroy_compress(&cinfo);
    free(out_buffer);
    return result;
}
