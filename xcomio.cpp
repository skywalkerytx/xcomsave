/*
XCom EW Saved Game Reader
Copyright(C) 2015

This program is free software; you can redistribute it and / or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110 - 1301 USA.
*/

/*
xcomio.cpp - Low-level operations on an xcom io buffer. 
*/

#include "xcom.h"
#include "xcomio.h"

#include <cstring>

namespace xcom
{
    void xcom_io::seek(seek_kind k, size_t offset)
    {
        switch (k)
        {
        case seek_kind::start:
            ptr_ = start_.get() + offset;
            break;
        case seek_kind::current:
            ptr_ += offset;
            break;
        case seek_kind::end:
            ptr_ = start_.get() + length_ + offset;
        }
    }

    int32_t xcom_io::read_int()
    {
        int32_t v = *(reinterpret_cast<const int32_t*>(ptr_));
        ptr_ += 4;
        return v;
    }

    float xcom_io::read_float()
    {
        float f = *(reinterpret_cast<const float*>(ptr_));
        ptr_ += 4;
        return f;
    }

    bool xcom_io::read_bool()
    {
        return read_int() != 0;
    }

    unsigned char xcom_io::read_byte()
    {
        return *ptr_++;
    }

    std::string xcom_io::read_string()
    {
        xcom_string s = read_unicode_string();
        if (s.is_wide)
        {
            throw format_exception(offset(), "Found UTF-16 string in unexpected location");
        }
        return s.str;
    }

    xcom_string xcom_io::read_unicode_string(bool throw_on_error)
    {
        int32_t length = read_int();
        if (length == 0) {
            return{ "", false };
        }

        if (length < 0) {

            // A UTF-16 encoded string.
            length = -length;

            // Sanity check
            if ((offset() + static_cast<size_t>(length)) > length_) {
                if (throw_on_error) {
                    throw format_exception(offset(), "read_string found an invalid string length.");
                }
                else {
                    return{ "", false };
                }
            }
            const char16_t *str = reinterpret_cast<const char16_t*>(ptr_);
            ptr_ += 2 * length;
            return{ util::utf16_to_utf8(str), true };
        }
        else {

            // Sanity check
            if ((offset() + static_cast<size_t>(length)) > length_) {
                if (throw_on_error) {
                    throw format_exception(offset(), "read_string found an invalid string length.");
                }
                else {
                    return{ "", false };
                }
            }

            const char *str = reinterpret_cast<const char *>(ptr_);
            size_t actual_length = strlen(str);

            // Double check the length matches what we read from the file,
            // considering the trailing null is counted in the length stored in
            // the file.
            if (actual_length != (length - 1))
            {
                if (throw_on_error) {
                    throw format_exception(offset(), "String mismatch: expected length %d but found %d\n", length, actual_length);
                }
                else {
                    return{ "", false };
                }
            }
            ptr_ += length;
            return{ util::iso8859_1_to_utf8(str), false };
        }
    }

    std::unique_ptr<unsigned char[]> xcom_io::read_raw_bytes(size_t count)
    {
        std::unique_ptr<unsigned char[]> buf = std::make_unique<unsigned char[]>(count);
        read_raw_bytes(count, buf.get());
        return buf;
    }

    void xcom_io::read_raw_bytes(size_t count, unsigned char *outp)
    {
        memcpy(outp, ptr_, count);
        ptr_ += count;
    }

    uint32_t xcom_io::crc(size_t length)
    {
        return util::crc32b(ptr_, length);
    }

    void xcom_io::ensure(size_t count)
    {
        std::ptrdiff_t current_count = offset();

        if ((current_count + count) > length_) {
            size_t new_length = length_ * 2;
            unsigned char * new_buffer = new unsigned char[new_length];
            memcpy(new_buffer, start_.get(), current_count);
            start_.reset(new_buffer);
            length_ = new_length;
            ptr_ = start_.get() + current_count;
        }
    }

    void xcom_io::write_string(const std::string& str)
    {
        write_unicode_string({ str, false });
    }

    void xcom_io::write_unicode_string(const xcom_string& s)
    {
        if (s.str.empty()) {
            // An empty string is just written as size 0
            write_int(0);
        }
        else if (s.is_wide) {
            std::u16string conv16 = util::utf8_to_utf16(s.str);
            ensure(conv16.length() * 2 + 6);
            // Write the length as a negative value, including the terminating
            // null character
            write_int((conv16.length() + 1) * -1);
            // Copy 2*length bytes of string data
            memcpy(ptr_, conv16.c_str(), conv16.length() * 2);
            ptr_ += conv16.length() * 2;

            // Write a double terminating null
            *ptr_++ = 0;
            *ptr_++ = 0;
            return;
        }
        else {
            // Looks like it's an ASCII/Latin-1 string.
            std::string conv = util::utf8_to_iso8859_1(s.str);
            ensure(conv.length() + 5);
            write_int(conv.length() + 1);
            memcpy(ptr_, conv.c_str(), conv.length());
            ptr_ += conv.length();
            *ptr_++ = 0;
        }
    }

    void xcom_io::write_int(int32_t val)
    {
        ensure(4);
        *(reinterpret_cast<int32_t*>(ptr_)) = val;
        ptr_ += 4;
    }


    void xcom_io::write_float(float val)
    {
        ensure(4);
        *(reinterpret_cast<float*>(ptr_)) = val;
        ptr_ += 4;
    }

    void xcom_io::write_bool(bool b)
    {
        write_int(b);
    }

    void xcom_io::write_byte(unsigned char c)
    {
        ensure(1);
        *ptr_++ = c;
    }

    void xcom_io::write_raw(unsigned char *buf, size_t len)
    {
        ensure(len);
        memcpy(ptr_, buf, len);
        ptr_ += len;
    }

} // namespace xcom
