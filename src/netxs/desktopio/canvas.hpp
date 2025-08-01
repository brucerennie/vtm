// Copyright (c) Dmitry Sapozhnikov
// Licensed under the MIT license.

#pragma once

#include "geometry.hpp"
#include "logger.hpp"

namespace netxs
{
    enum class svga
    {
        vt_2D,
        vtrgb,
        vt256,
        vt16 ,
        nt16 ,
        dtvt ,
    };

    namespace zpos
    {
        static constexpr auto backmost = -1;
        static constexpr auto plain    = 0;
        static constexpr auto topmost  = 1;
    };

    namespace unln
    {
        static constexpr auto none   = 0;
        static constexpr auto line   = 1;
        static constexpr auto biline = 2;
        static constexpr auto wavy   = 3;
        static constexpr auto dotted = 4;
        static constexpr auto dashed = 5;
    }

    namespace text_cursor
    {
        static constexpr auto none      = 0;
        static constexpr auto underline = 1;
        static constexpr auto block     = 2;
        static constexpr auto I_bar     = 3;
    }

    enum tint
    {
        blackdk, reddk, greendk, yellowdk, bluedk, magentadk, cyandk, whitedk,
        blacklt, redlt, greenlt, yellowlt, bluelt, magentalt, cyanlt, whitelt,
        pureblack   = 16 + 36 * 0 + 6 * 0 + 0,
        purewhite   = 16 + 36 * 5 + 6 * 5 + 5,
        purered     = 16 + 36 * 5 + 6 * 0 + 0,
        puregreen   = 16 + 36 * 0 + 6 * 5 + 0,
        pureblue    = 16 + 36 * 0 + 6 * 0 + 5,
        pureyellow  = 16 + 36 * 5 + 6 * 5 + 0,
        purecyan    = 16 + 36 * 0 + 6 * 5 + 5,
        puremagenta = 16 + 36 * 5 + 6 * 0 + 5,
    };

    struct tint16
    {
        enum : si32
        {
            blackdk,  // 0
            blacklt,  // 1
            graydk,   // 2
            graylt,   // 3
            whitedk,  // 4
            whitelt,  // 5
            reddk,    // 6
            bluedk,   // 7
            greendk,  // 8
            yellowdk, // 9
            magentalt,// 10
            cyanlt,   // 11
            redlt,    // 12
            bluelt,   // 13
            greenlt,  // 14
            yellowlt, // 15
        };
    };

    // canvas: 8-bit ARGB.
    union argb
    {
        ui32                        token;
        struct { byte b, g, r, a; } chan;

        constexpr argb()
            : token{ 0 }
        { }
        constexpr argb(argb const&) = default;
        template<class T, class A = byte>
        constexpr argb(T r, T g, T b, A a = 0xff)
            : chan{ static_cast<byte>(b),
                    static_cast<byte>(g),
                    static_cast<byte>(r),
                    static_cast<byte>(a) }
        { }
        constexpr argb(fp32 r, fp32 g, fp32 b, fp32 a)
            : chan{ netxs::saturate_cast<byte>(b * 255),
                    netxs::saturate_cast<byte>(g * 255),
                    netxs::saturate_cast<byte>(r * 255),
                    netxs::saturate_cast<byte>(a * 255) }
        { }
        template<class T>
        constexpr argb(T const& c)
            : argb(c.b, c.g, c.r, c.a)
        { }
        constexpr argb(ui32 c)
            : token{ netxs::letoh(c) }
        { }
        constexpr argb(si32 c)
            : argb{ (ui32)c }
        { }
        constexpr argb(tint c)
            : argb{ vt256[c] }
        { }
        argb(fifo& q)
        {
            static constexpr auto mode_RGB = 2;
            static constexpr auto mode_256 = 5;
            auto mode = q.rawarg(mode_RGB);
            if (fifo::issub(mode))
            {
                switch (fifo::desub(mode))
                {
                    case mode_RGB:
                    {
                        auto r = q.subarg(-1); // Skip the case with color space: \x1b[38:2::255:255:255:::m.
                        chan.r = (byte)(r == -1 ? q.subarg(0) : r);
                        chan.g = (byte)(q.subarg(0));
                        chan.b = (byte)(q.subarg(0));
                        chan.a = (byte)(q.subarg(0xFF));
                        break;
                    }
                    case mode_256:
                        token = netxs::letoh(vt256[q.subarg(0)]);
                        break;
                    default:
                        break;
                }
            }
            else
            {
                switch (mode)
                {
                    case mode_RGB:
                        chan.r = (byte)(q(0));
                        chan.g = (byte)(q(0));
                        chan.b = (byte)(q(0));
                        chan.a = 0xFF;
                        break;
                    case mode_256:
                        token = netxs::letoh(vt256[q(0)]);
                        break;
                    default:
                        break;
                }
            }
        }

        constexpr argb& operator = (argb const&) = default;
        constexpr explicit operator bool () const
        {
            return token;
        }
        constexpr auto operator == (argb c) const
        {
            return token == c.token;
        }
        constexpr auto operator != (argb c) const
        {
            return !operator==(c);
        }
        auto& swap_rb()
        {
            token = ((token >>  0) & 0xFF'00'FF'00) |
                    ((token >> 16) & 0x00'00'00'FF) |
                    ((token << 16) & 0x00'FF'00'00);
            return *this;
        }
        static auto swap_rb(ui32 c)
        {
            return ((c >>  0) & 0x00'FF'00) |
                   ((c >> 16) & 0x00'00'FF) |
                   ((c << 16) & 0xFF'00'00);
        }
        // argb: Set all channels to zero.
        void wipe()
        {
            token = 0;
        }
        // argb: Set color to opaque black.
        void rst()
        {
            static constexpr auto colorblack = argb{ 0xFF000000 };
            token = colorblack.token;
        }
        // argb: Are the colors alpha blenable?
        auto is_alpha_blendable() const
        {
            return chan.a && chan.a != 0xFF;
        }
        // argb: Set alpha channel.
        auto& alpha(si32 k)
        {
            chan.a = (byte)k;
            return *this;
        }
        auto& alpha(fp32 k)
        {
            chan.a = (byte)std::clamp(k * 255.f, 0.f, 255.f);
            return *this;
        }
        // argb: Sum alpha channel.
        auto& alpha_sum(si32 k)
        {
            chan.a = (byte)std::clamp(chan.a + k, 0, 255);
            return *this;
        }
        // argb: Sum alpha channel.
        auto& alpha_sum(fp32 k)
        {
            chan.a = (byte)std::clamp(chan.a + k * 255.f, 0.f, 255.f);
            return *this;
        }
        // argb: Sum alpha channels.
        static void alpha_mix(si32 src, byte& dst)
        {
            dst = (byte)std::clamp(src + dst, 0, 255);
        }
        // argb: Return alpha channel.
        auto alpha() const
        {
            return chan.a;
        }
        // argb: Colourimetric (perceptual luminance-preserving) conversion to greyscale.
        constexpr auto luma() const
        {
            auto r = (token >> 16) & 0xFF;
            auto g = (token >>  8) & 0xFF;
            auto b = (token >>  0) & 0xFF;
            return static_cast<byte>(0.2627f * r + 0.6780f * g + 0.0593f * b);
        }
        static constexpr auto luma(si32 r, si32 g, si32 b)
        {
            return static_cast<byte>(0.2627f * r + 0.6780f * g + 0.0593f * b);
        }
        void grayscale()
        {
            auto l = luma();
            chan.r = l;
            chan.g = l;
            chan.b = l;
        }
        // argb: Return 256-color 6x6x6 cube.
        auto to_256cube() const
        {
            auto clr = 0;
            if (chan.r == chan.g && chan.r == chan.b)
            {
                clr = 232 + ((chan.r * 24) >> 8);
            }
            else
            {
                clr = 16 + 36 * ((chan.r * 6) >> 8)
                         +  6 * ((chan.g * 6) >> 8)
                              + ((chan.b * 6) >> 8);
            }
            return (byte)clr;
        }
        // argb: Equal both to their average.
        void avg(argb& c)
        {
            chan.r = c.chan.r = (byte)(((ui32)chan.r + c.chan.r) >> 1);
            chan.g = c.chan.g = (byte)(((ui32)chan.g + c.chan.g) >> 1);
            chan.b = c.chan.b = (byte)(((ui32)chan.b + c.chan.b) >> 1);
        }
        // argb: One-side alpha blending ARGB colors.
        void inline mix_one(argb c)
        {
            if (c.chan.a == 0xFF)
            {
                chan = c.chan;
            }
            else if (c.chan.a)
            {
                auto blend = [](auto c1, auto c2, auto alpha)
                {
                    return (byte)(((c1 << 8) + (c2 - c1) * alpha) >> 8);
                };
                chan.r = blend(chan.r, c.chan.r, c.chan.a);
                chan.g = blend(chan.g, c.chan.g, c.chan.a);
                chan.b = blend(chan.b, c.chan.b, c.chan.a);

                //if (!chan.a) chan.a = c.chan.a;
            }
        }
        // argb: Alpha blending ARGB colors.
        void inline mix(argb c)
        {
            if (c.chan.a == 0xFF)
            {
                chan = c.chan;
            }
            else if (c.chan.a)
            {
                //todo consider premultiplied alpha
                auto a1 = ui32{ chan.a };
                auto a2 = ui32{ c.chan.a };
                auto a = ((a2 + a1) << 8) - a1 * a2;
                auto blend2 = [&](auto c1, auto c2)
                {
                    auto t = c1 * a1;
                    auto d = ((c2 * a2 + t) << 8) - t * a2;
                    return (byte)(d / a);
                    //return (((c2 * a2 + t) << 8) - t * a2) / a;
                };
                chan.r = blend2(chan.r, c.chan.r);
                chan.g = blend2(chan.g, c.chan.g);
                chan.b = blend2(chan.b, c.chan.b);
                chan.a = (byte)(a >> 8);
            }
        }
        // argb: Alpha blending ARGB colors.
        void blend(argb c)
        {
            mix(c);
        }
        // argb: ARGB transitional blending. Level = 0: equals c1, level = 256: equals c2.
        static auto transit(argb c1, argb c2, si32 level)
        {
            auto inverse = 256 - level;
            return argb{ (c2.chan.r * level + c1.chan.r * inverse) >> 8,
                         (c2.chan.g * level + c1.chan.g * inverse) >> 8,
                         (c2.chan.b * level + c1.chan.b * inverse) >> 8,
                         (c2.chan.a * level + c1.chan.a * inverse) >> 8 };
        }
        static auto transit(argb c1, argb c2, fp32 level)
        {
            auto inverse = 1.f - level;
            return argb{ (byte)std::clamp(c2.chan.r * level + c1.chan.r * inverse, 0.f, 255.f),
                         (byte)std::clamp(c2.chan.g * level + c1.chan.g * inverse, 0.f, 255.f),
                         (byte)std::clamp(c2.chan.b * level + c1.chan.b * inverse, 0.f, 255.f),
                         (byte)std::clamp(c2.chan.a * level + c1.chan.a * inverse, 0.f, 255.f) };
        }
        // argb: Alpha blending ARGB colors.
        void inline mix(argb c, si32 alpha)
        {
            if (alpha == 255) chan = c.chan;
            else if (alpha)
            {
                auto na = 256 - alpha;
                chan.r = (byte)((c.chan.r * alpha + chan.r * na) >> 8);
                chan.g = (byte)((c.chan.g * alpha + chan.g * na) >> 8);
                chan.b = (byte)((c.chan.b * alpha + chan.b * na) >> 8);
                chan.a = (byte)((c.chan.a * alpha + chan.a * na) >> 8);
            }
        }
        // argb: Rough alpha blending ARGB colors.
        //void mix_alpha(argb c)
        //{
        //    auto blend = [](auto const& c1, auto const& c2, auto const& alpha)
        //    {
        //        return ((c1 << 8) + (c2 - c1) * alpha) >> 8;
        //    };
        //    auto norm = [](auto const& c2, auto const& alpha)
        //    {
        //        return (c2 * alpha) >> 8;
        //    };
        //
        //    if (chan.a)
        //    {
        //        if (c.chan.a)
        //        {
        //            auto& a1 = chan.a;
        //            auto& a2 = c.chan.a;
        //            chan.r = blend(norm(c.chan.r, a2), chan.r, a1);
        //            chan.g = blend(norm(c.chan.g, a2), chan.g, a1);
        //            chan.b = blend(norm(c.chan.b, a2), chan.b, a1);
        //            chan.a = c.chan.a;
        //        }
        //    }
        //    else
        //    {
        //        chan = c.chan;
        //    }
        //}
        //// argb: Are the colors identical.
        //bool like(argb c) const
        //{
        //    static constexpr auto k = ui32{ 0b11110000 };
        //    static constexpr auto threshold = ui32{ 0x00 + k << 16 + k << 8 + k };
        //    return (token & threshold) == (c.token & threshold);
        //}
        // argb: Shift color.
        void xlight(si32 factor = 1)
        {
            if (chan.a == 255)
            {
                if (luma() > 140)
                {
                    auto k = (byte)std::clamp(64 * factor, 0, 0xFF);
                    chan.r = chan.r < k ? 0x00 : chan.r - k;
                    chan.g = chan.g < k ? 0x00 : chan.g - k;
                    chan.b = chan.b < k ? 0x00 : chan.b - k;
                }
                else
                {
                    auto k = (byte)std::clamp(48 * factor, 0, 0xFF);
                    chan.r = chan.r > 0xFF - k ? 0xFF : chan.r + k;
                    chan.g = chan.g > 0xFF - k ? 0xFF : chan.g + k;
                    chan.b = chan.b > 0xFF - k ? 0xFF : chan.b + k;
                }
            }
            else if (chan.a < 2)
            {
                auto k = (byte)std::clamp(48 * factor, 0, 0xFF);
                chan.r = k;
                chan.g = k;
                chan.b = k;
                chan.a = (byte)std::min(255, 2 * k);
            }
            else
            {
                auto r = (si32)chan.r * chan.a / 256;
                auto g = (si32)chan.g * chan.a / 256;
                auto b = (si32)chan.b * chan.a / 256;
                if (luma(r, g, b) > 140)
                {
                    auto k = (byte)std::clamp(64 * factor, 0, 0xFF);
                    chan.r = chan.r < k ? 0x00 : chan.r - k;
                    chan.g = chan.g < k ? 0x00 : chan.g - k;
                    chan.b = chan.b < k ? 0x00 : chan.b - k;
                    chan.a = chan.a > 0xFF - k ? 0xFF : chan.a + k;
                }
                else
                {
                    auto k = (byte)std::clamp(48 * factor, 0, 0xFF);
                    chan.r = chan.r > 0xFF - k ? 0xFF : chan.r + k;
                    chan.g = chan.g > 0xFF - k ? 0xFF : chan.g + k;
                    chan.b = chan.b > 0xFF - k ? 0xFF : chan.b + k;
                    chan.a = chan.a > 0xFF - k ? 0xFF : chan.a + k;
                }
            }
        }
        // argb: Shift color pair.
        void xlight(si32 factor, argb& second)
        {
            if (chan.a == 255)
            {
                if (luma() > 140)
                {
                    auto k = (byte)std::clamp(64 * factor, 0, 0xFF);
                    chan.r = chan.r < k ? 0x00 : chan.r - k;
                    chan.g = chan.g < k ? 0x00 : chan.g - k;
                    chan.b = chan.b < k ? 0x00 : chan.b - k;
                    second.chan.r = second.chan.r < k ? 0x00 : second.chan.r - k;
                    second.chan.g = second.chan.g < k ? 0x00 : second.chan.g - k;
                    second.chan.b = second.chan.b < k ? 0x00 : second.chan.b - k;
                }
                else
                {
                    auto k = (byte)std::clamp(48 * factor, 0, 0xFF);
                    chan.r = chan.r > 0xFF - k ? 0xFF : chan.r + k;
                    chan.g = chan.g > 0xFF - k ? 0xFF : chan.g + k;
                    chan.b = chan.b > 0xFF - k ? 0xFF : chan.b + k;
                    second.chan.r = second.chan.r > 0xFF - k ? 0xFF : second.chan.r + k;
                    second.chan.g = second.chan.g > 0xFF - k ? 0xFF : second.chan.g + k;
                    second.chan.b = second.chan.b > 0xFF - k ? 0xFF : second.chan.b + k;
                }
            }
            else if (chan.a < 2)
            {
                auto k = (byte)std::clamp(48 * factor, 0, 0xFF);
                chan.r = k;
                chan.g = k;
                chan.b = k;
                chan.a = (byte)std::min(255, 2 * k);
                second.chan.r = second.chan.r > 0xFF - k ? 0xFF : second.chan.r + k;
                second.chan.g = second.chan.g > 0xFF - k ? 0xFF : second.chan.g + k;
                second.chan.b = second.chan.b > 0xFF - k ? 0xFF : second.chan.b + k;
            }
            else
            {
                auto r = (si32)chan.r * chan.a / 256;
                auto g = (si32)chan.g * chan.a / 256;
                auto b = (si32)chan.b * chan.a / 256;
                if (luma(r, g, b) > 140)
                {
                    auto k = (byte)std::clamp(64 * factor, 0, 0xFF);
                    chan.r = chan.r < k ? 0x00 : chan.r - k;
                    chan.g = chan.g < k ? 0x00 : chan.g - k;
                    chan.b = chan.b < k ? 0x00 : chan.b - k;
                    //chan.a = chan.a > 0xFF - k ? 0xFF : chan.a + k;
                    second.chan.r = second.chan.r < k ? 0x00 : second.chan.r - k;
                    second.chan.g = second.chan.g < k ? 0x00 : second.chan.g - k;
                    second.chan.b = second.chan.b < k ? 0x00 : second.chan.b - k;
                }
                else
                {
                    auto k = (byte)std::clamp(48 * factor, 0, 0xFF);
                    chan.r = chan.r > 0xFF - k ? 0xFF : chan.r + k;
                    chan.g = chan.g > 0xFF - k ? 0xFF : chan.g + k;
                    chan.b = chan.b > 0xFF - k ? 0xFF : chan.b + k;
                    // xlight for 0x80'000000 is invisible on purewhite os desktop
                    //chan.a = chan.a > 0xFF - k ? 0xFF : chan.a + k;
                    second.chan.r = second.chan.r > 0xFF - k ? 0xFF : second.chan.r + k;
                    second.chan.g = second.chan.g > 0xFF - k ? 0xFF : second.chan.g + k;
                    second.chan.b = second.chan.b > 0xFF - k ? 0xFF : second.chan.b + k;
                }
            }
        }
        // argb: Darken the color.
        auto shadow(byte k = 39)
        {
            chan.r = chan.r < k ? 0x00 : chan.r - k;
            chan.g = chan.g < k ? 0x00 : chan.g - k;
            chan.b = chan.b < k ? 0x00 : chan.b - k;
            return *this;
        }
        // argb: Faint color.
        auto faint()
        {
            chan.r >>= 1;
            chan.g >>= 1;
            chan.b >>= 1;
            return *this;
        }
        // argb: Lighten the color.
        void bright(si32 factor = 1)
        {
            auto k = (byte)std::clamp(39 * factor, 0, 0xFF);
            chan.r = chan.r > 0xFF - k ? 0xFF : chan.r + k;
            chan.g = chan.g > 0xFF - k ? 0xFF : chan.g + k;
            chan.b = chan.b > 0xFF - k ? 0xFF : chan.b + k;
        }
        // argb: Invert the color.
        void invert()
        {
            static constexpr auto pureblack = argb{ 0xFF000000 };
            static constexpr auto antiwhite = argb{ 0x00FFFFFF };

            token = (token & pureblack.token) | ~(token & antiwhite.token);
        }
        // argb: Print channel values.
        auto str() const
        {
            return "{" + std::to_string(chan.r) + ","
                       + std::to_string(chan.g) + ","
                       + std::to_string(chan.b) + ","
                       + std::to_string(chan.a) + "}";
        }

        static constexpr auto default_color = 0x00'FF'FF'FF;
        static constexpr auto active_transparent = 0x01'000000;

        template<si32 i>
        static constexpr ui32 _vt16 = // Compile-time value assigning (sorted by enum).
            i == tint::blackdk   ? 0xFF'10'10'10 :
            i == tint::reddk     ? 0xFF'C4'0F'1F :
            i == tint::greendk   ? 0xFF'12'A1'0E :
            i == tint::yellowdk  ? 0xFF'C0'9C'00 :
            i == tint::bluedk    ? 0xFF'00'37'DB :
            i == tint::magentadk ? 0xFF'87'17'98 :
            i == tint::cyandk    ? 0xFF'3B'96'DD :
            i == tint::whitedk   ? 0xFF'BB'BB'BB :
            i == tint::blacklt   ? 0xFF'75'75'75 :
            i == tint::redlt     ? 0xFF'E6'48'56 :
            i == tint::greenlt   ? 0xFF'15'C6'0C :
            i == tint::yellowlt  ? 0xFF'F8'F1'A5 :
            i == tint::bluelt    ? 0xFF'3A'78'FF :
            i == tint::magentalt ? 0xFF'B3'00'9E :
            i == tint::cyanlt    ? 0xFF'60'D6'D6 :
            i == tint::whitelt   ? 0xFF'F3'F3'F3 : 0;
        template<si32 i>
        static constexpr ui32 _vtm16 = // Compile-time value assigning (sorted by enum).
            i == tint16::blackdk   ? 0xFF'00'00'00    :
            i == tint16::blacklt   ? 0xFF'20'20'20    :
            i == tint16::graydk    ? 0xFF'50'50'50    :
            i == tint16::graylt    ? 0xFF'80'80'80    :
            i == tint16::whitedk   ? 0xFF'D0'D0'D0    :
            i == tint16::whitelt   ? 0xFF'FF'FF'FF    :
            i == tint16::reddk     ? _vt16<reddk>     :
            i == tint16::bluedk    ? _vt16<bluedk>    :
            i == tint16::greendk   ? _vt16<greendk>   :
            i == tint16::yellowdk  ? _vt16<yellowdk>  :
            i == tint16::magentalt ? _vt16<magentalt> :
            i == tint16::cyanlt    ? _vt16<cyanlt>    :
            i == tint16::redlt     ? _vt16<redlt>     :
            i == tint16::bluelt    ? _vt16<bluelt>    :
            i == tint16::greenlt   ? _vt16<greenlt>   :
            i == tint16::yellowlt  ? _vt16<yellowlt>  : 0;

        static constexpr auto vtm16 = std::to_array(
        {
            _vtm16<0>, _vtm16<1>, _vtm16<2>,  _vtm16<3>,  _vtm16<4>,  _vtm16<5>,  _vtm16<6>,  _vtm16<7>,
            _vtm16<8>, _vtm16<9>, _vtm16<10>, _vtm16<11>, _vtm16<12>, _vtm16<13>, _vtm16<14>, _vtm16<15>,
        });
        static constexpr auto vga16 = std::to_array(
        {
            _vt16<blackdk>, _vt16<bluedk>, _vt16<greendk>, _vt16<cyandk>, _vt16<reddk>, _vt16<magentadk>, _vt16<yellowdk>, _vt16<whitedk>,
            _vt16<blacklt>, _vt16<bluelt>, _vt16<greenlt>, _vt16<cyanlt>, _vt16<redlt>, _vt16<magentalt>, _vt16<yellowlt>, _vt16<whitelt>,
        });
        static constexpr auto vt256 = std::to_array(
        {
            _vt16<0>, _vt16<1>, _vt16<2>,  _vt16<3>,  _vt16<4>,  _vt16<5>,  _vt16<6>,  _vt16<7>,
            _vt16<8>, _vt16<9>, _vt16<10>, _vt16<11>, _vt16<12>, _vt16<13>, _vt16<14>, _vt16<15>,

            // 6×6×6 RGB-cube (216 colors), index = 16 + 36r + 6g + b, r,g,b=[0, 5]
            0xFF000000, 0xFF00005F, 0xFF000087, 0xFF0000AF, 0xFF0000D7, 0xFF0000FF,
            0xFF005F00, 0xFF005F5F, 0xFF005F87, 0xFF005FAF, 0xFF005FD7, 0xFF005FFF,
            0xFF008700, 0xFF00875F, 0xFF008787, 0xFF0087AF, 0xFF0087D7, 0xFF0087FF,
            0xFF00AF00, 0xFF00AF5F, 0xFF00AF87, 0xFF00AFAF, 0xFF00AFD7, 0xFF00AFFF,
            0xFF00D700, 0xFF00D75F, 0xFF00D787, 0xFF00D7AF, 0xFF00D7D7, 0xFF00D7FF,
            0xFF00FF00, 0xFF00FF5F, 0xFF00FF87, 0xFF00FFAF, 0xFF00FFD7, 0xFF00FFFF,

            0xFF5F0000, 0xFF5F005F, 0xFF5F0087, 0xFF5F00AF, 0xFF5F00D7, 0xFF5F00FF,
            0xFF5F5F00, 0xFF5F5F5F, 0xFF5F5F87, 0xFF5F5FAF, 0xFF5F5FD7, 0xFF5F5FFF,
            0xFF5F8700, 0xFF5F875F, 0xFF5F8787, 0xFF5F87AF, 0xFF5F87D7, 0xFF5F87FF,
            0xFF5FAF00, 0xFF5FAF5F, 0xFF5FAF87, 0xFF5FAFAF, 0xFF5FAFD7, 0xFF5FAFFF,
            0xFF5FD700, 0xFF5FD75F, 0xFF5FD787, 0xFF5FD7AF, 0xFF5FD7D7, 0xFF5FD7FF,
            0xFF5FFF00, 0xFF5FFF5F, 0xFF5FFF87, 0xFF5FFFAF, 0xFF5FFFD7, 0xFF5FFFFF,

            0xFF870000, 0xFF87005F, 0xFF870087, 0xFF8700AF, 0xFF8700D7, 0xFF8700FF,
            0xFF875F00, 0xFF875F5F, 0xFF875F87, 0xFF875FAF, 0xFF875FD7, 0xFF875FFF,
            0xFF878700, 0xFF87875F, 0xFF878787, 0xFF8787AF, 0xFF8787D7, 0xFF8787FF,
            0xFF87AF00, 0xFF87AF5F, 0xFF87AF87, 0xFF87AFAF, 0xFF87AFD7, 0xFF87AFFF,
            0xFF87D700, 0xFF87D75F, 0xFF87D787, 0xFF87D7AF, 0xFF87D7D7, 0xFF87D7FF,
            0xFF87FF00, 0xFF87FF5F, 0xFF87FF87, 0xFF87FFAF, 0xFF87FFD7, 0xFF87FFFF,

            0xFFAF0000, 0xFFAF005F, 0xFFAF0087, 0xFFAF00AF, 0xFFAF00D7, 0xFFAF00FF,
            0xFFAF5F00, 0xFFAF5F5F, 0xFFAF5F87, 0xFFAF5FAF, 0xFFAF5FD7, 0xFFAF5FFF,
            0xFFAF8700, 0xFFAF875F, 0xFFAF8787, 0xFFAF87AF, 0xFFAF87D7, 0xFFAF87FF,
            0xFFAFAF00, 0xFFAFAF5F, 0xFFAFAF87, 0xFFAFAFAF, 0xFFAFAFD7, 0xFFAFAFFF,
            0xFFAFD700, 0xFFAFD75F, 0xFFAFD787, 0xFFAFD7AF, 0xFFAFD7D7, 0xFFAFD7FF,
            0xFFAFFF00, 0xFFAFFF5F, 0xFFAFFF87, 0xFFAFFFAF, 0xFFAFFFD7, 0xFFAFFFFF,

            0xFFD70000, 0xFFD7005F, 0xFFD70087, 0xFFD700AF, 0xFFD700D7, 0xFFD700FF,
            0xFFD75F00, 0xFFD75F5F, 0xFFD75F87, 0xFFD75FAF, 0xFFD75FD7, 0xFFD75FFF,
            0xFFD78700, 0xFFD7875F, 0xFFD78787, 0xFFD787AF, 0xFFD787D7, 0xFFD787FF,
            0xFFD7AF00, 0xFFD7AF5F, 0xFFD7AF87, 0xFFD7AFAF, 0xFFD7AFD7, 0xFFD7AFFF,
            0xFFD7D700, 0xFFD7D75F, 0xFFD7D787, 0xFFD7D7AF, 0xFFD7D7D7, 0xFFD7D7FF,
            0xFFD7FF00, 0xFFD7FF5F, 0xFFD7FF87, 0xFFD7FFAF, 0xFFD7FFD7, 0xFFD7FFFF,

            0xFFFF0000, 0xFFFF005F, 0xFFFF0087, 0xFFFF00AF, 0xFFFF00D7, 0xFFFF00FE,
            0xFFFF5F00, 0xFFFF5F5F, 0xFFFF5F87, 0xFFFF5FAF, 0xFFFF5FD7, 0xFFFF5FFE,
            0xFFFF8700, 0xFFFF875F, 0xFFFF8787, 0xFFFF87AF, 0xFFFF87D7, 0xFFFF87FE,
            0xFFFFAF00, 0xFFFFAF5F, 0xFFFFAF87, 0xFFFFAFAF, 0xFFFFAFD7, 0xFFFFAFFE,
            0xFFFFD700, 0xFFFFD75F, 0xFFFFD787, 0xFFFFD7AF, 0xFFFFD7D7, 0xFFFFD7FE,
            0xFFFFFF00, 0xFFFFFF5F, 0xFFFFFF87, 0xFFFFFFAF, 0xFFFFFFD7, 0xFFFFFFFF,
            // Grayscale colors, 24 steps
            0xFF080808, 0xFF121212, 0xFF1C1C1C, 0xFF262626, 0xFF303030, 0xFF3A3A3A,
            0xFF444444, 0xFF4E4E4E, 0xFF585858, 0xFF626262, 0xFF6C6C6C, 0xFF767676,
            0xFF808080, 0xFF8A8A8A, 0xFF949494, 0xFF9E9E9E, 0xFFA8A8A8, 0xFFB2B2B2,
            0xFFBCBCBC, 0xFFC6C6C6, 0xFFD0D0D0, 0xFFDADADA, 0xFFE4E4E4, 0xFFEEEEEE,
        });
        friend auto& operator << (std::ostream& s, argb c)
        {
            return s << "{" << (si32)c.chan.r
                     << "," << (si32)c.chan.g
                     << "," << (si32)c.chan.b
                     << "," << (si32)c.chan.a
                     << "}";
        }
        static auto set_vtm16_palette(auto proc)
        {
            for (auto i = 0; i < 16; i++)
            {
                proc(i, argb::vtm16[i]);
            }
        }
        auto lookup(auto& fast, auto&& palette) const
        {
            auto head = fast.begin();
            auto tail = fast.end();
            auto init = head;
            while (head != tail) // Look in static table.
            {
                auto& next = *head;
                if (next.first == token)
                {
                    if (head == init) return next.second;
                    else
                    {
                        auto& prev = *(--head);
                        std::swap(next, prev); // Sort by hits. !! Not thread-safe.
                        return prev.second;
                    }
                }
                ++head;
            }
            auto dist = [](si32 c1, si32 c2)
            {
                auto dr = ((c1 & 0x0000FF) >> 0)  - ((c2 & 0x0000FF) >> 0);
                auto dg = ((c1 & 0x00FF00) >> 8)  - ((c2 & 0x00FF00) >> 8);
                auto db = ((c1 & 0xFF0000) >> 16) - ((c2 & 0xFF0000) >> 16);
                return (ui32)(dr * dr + dg * dg + db * db); // std::abs(dr) + std::abs(dg) + std::abs(db);
            };
            auto max = 1368u; // Minimal distance: between greenlt and greendk - 1.
            auto hit = std::pair{ max, 0 };
            for (auto i = 0; i < (si32)std::size(palette); i++) // Find the nearest.
            {
                if (auto d = dist(palette[i], token))
                {
                    if (d < hit.first) hit = { d, i };
                }
                else return i;
            }
            if (hit.first == max) // Fallback to grayscale.
            {
                auto l = luma();
                     if (l < 42)  hit.second = tint16::blacklt;
                else if (l < 90)  hit.second = tint16::graydk;
                else if (l < 170) hit.second = tint16::graylt;
                else if (l < 240) hit.second = tint16::whitedk;
                else              hit.second = tint16::whitelt;
            }
            return hit.second;
        }
        auto to_vga16(bool fg = true) const // argb: 4-bit Foreground color (vtm 16-color mode).
        {
            static auto cache_fg = []
            {
                auto table = std::vector<std::pair<ui32, si32>>{};
                for (auto i = 0; i < 16; i++) table.push_back({ argb::vt256[i], i });
                table.insert(table.end(),
                {
                    { 0xFF'ff'ff'ff, tint::whitelt   },
                    { 0xff'aa'aa'aa, tint::whitedk   },
                    { 0xff'80'80'80, tint::whitedk   },
                    { 0xff'55'55'55, tint::blacklt   },
                    { 0xFF'00'00'00, tint::blackdk   },

                    { 0xFF'55'00'00, tint::reddk     },
                    { 0xFF'80'00'00, tint::reddk     },
                    { 0xFF'aa'00'00, tint::reddk     },
                    { 0xFF'ff'00'00, tint::redlt     },

                    { 0xFF'00'00'55, tint::bluedk    },
                    { 0xFF'00'00'80, tint::bluedk    },
                    { 0xFF'00'00'aa, tint::bluedk    },
                    { 0xFF'00'00'ff, tint::bluelt    },

                    { 0xFF'00'aa'00, tint::greendk   },
                    { 0xFF'00'80'00, tint::greendk   },
                    { 0xFF'00'ff'00, tint::greenlt   },
                    { 0xFF'55'ff'55, tint::greenlt   },

                    { 0xFF'80'00'80, tint::magentadk },
                    { 0xFF'aa'00'aa, tint::magentadk },
                    { 0xFF'ff'55'ff, tint::magentalt },
                    { 0xFF'ff'00'ff, tint::magentalt },

                    { 0xFF'00'80'80, tint::cyandk    },
                    { 0xFF'00'aa'aa, tint::cyandk    },
                    { 0xFF'55'ff'ff, tint::cyanlt    },
                    { 0xFF'00'ff'ff, tint::cyanlt    },

                    { 0xFF'aa'55'00, tint::yellowdk  },
                    { 0xFF'80'80'00, tint::yellowdk  },
                    { 0xFF'ff'ff'00, tint::yellowlt  },
                    { 0xFF'ff'ff'55, tint::yellowlt  },
                });
                return table;
            }();
            static auto cache_bg = cache_fg;
            auto& cache = fg ? cache_fg : cache_bg; // Fg and Bg are sorted differently.
            auto c = lookup(cache, std::span{ argb::vt256.data(), 16 });
            return netxs::swap_bits<0, 2>(c); // ANSI<->DOS color scheme reindex.
        }
        auto to_vtm16(bool fg = true) const // argb: 4-bit Foreground color (vtm 16-color palette).
        {
            static auto cache_fg = []
            {
                auto table = std::vector<std::pair<ui32, si32>>{};
                for (auto i = 0u; i < std::size(argb::vtm16); i++) table.push_back({ argb::vtm16[i], i });
                table.insert(table.end(),
                {
                    { 0xFF'55'00'00,                 tint16::reddk     },
                    { 0xFF'aa'00'00,                 tint16::reddk     },
                    { 0xFF'80'00'00,                 tint16::reddk     },
                    { 0xFF'ff'00'00,                 tint16::redlt     },

                    { argb::vt256[tint::magentadk],  tint16::reddk     },
                    { 0xFF'80'00'80,                 tint16::reddk     },
                    { 0xFF'ff'55'ff,                 tint16::magentalt },
                    { 0xFF'ff'00'ff,                 tint16::magentalt },

                    { argb::vt256[tint::cyandk],     tint16::bluelt    },
                    { 0xFF'00'80'80,                 tint16::bluelt    },
                    { 0xFF'00'aa'aa,                 tint16::bluelt    },
                    { 0xFF'55'ff'ff,                 tint16::cyanlt    },
                    { 0xFF'00'ff'ff,                 tint16::cyanlt    },

                    { 0xFF'ff'ff'ff,                 tint16::whitelt   },
                    { 0xff'aa'aa'aa,                 tint16::whitedk   },
                    { 0xff'80'80'80,                 tint16::graylt    },
                    { 0xff'55'55'55,                 tint16::graydk    },
                    { 0xFF'00'00'00,                 tint16::blackdk   },

                    { 0xFF'00'00'55,                 tint16::bluedk    },
                    { 0xFF'00'00'80,                 tint16::bluedk    },
                    { 0xFF'00'00'aa,                 tint16::bluedk    },
                    { 0xFF'00'00'ff,                 tint16::bluelt    },

                    { 0xFF'00'aa'00,                 tint16::greendk   },
                    { 0xFF'00'80'00,                 tint16::greendk   },
                    { 0xFF'00'ff'00,                 tint16::greenlt   },
                    { 0xFF'55'ff'55,                 tint16::greenlt   },

                    { 0xFF'aa'55'00,                 tint16::yellowdk  },
                    { 0xFF'80'80'00,                 tint16::yellowdk  },
                    { 0xFF'ff'ff'00,                 tint16::yellowlt  },
                    { 0xFF'ff'ff'55,                 tint16::yellowlt  },
                });
                return table;
            }();
            static auto cache_bg = cache_fg;
            auto& cache = fg ? cache_fg : cache_bg; // Fg and Bg are sorted differently.
            return lookup(cache, argb::vtm16);
        }
        auto to_vtm8() const // argb: 3-bit Background color (vtm 8-color palette).
        {
            static auto cache = []
            {
                auto table = std::vector<std::pair<ui32, si32>>{};
                for (auto i = 0u; i < std::size(argb::vtm16) / 2; i++) table.push_back({ argb::vtm16[i], i });
                table.insert(table.end(),
                {
                    { argb::vt256[tint::bluelt   ],  tint16::bluedk  },
                    { argb::vt256[tint::redlt    ],  tint16::reddk   },
                    { argb::vt256[tint::cyanlt   ],  tint16::whitedk },
                    { argb::vt256[tint::cyandk   ],  tint16::graylt  },
                    { argb::vt256[tint::greenlt  ],  tint16::graylt  },
                    { argb::vt256[tint::greendk  ],  tint16::graydk  },
                    { argb::vt256[tint::yellowdk ],  tint16::graydk  },
                    { argb::vt256[tint::yellowlt ],  tint16::whitelt },
                    { argb::vt256[tint::magentalt],  tint16::reddk   },
                    { argb::vt256[tint::magentadk],  tint16::reddk   },
                    { 0xff'00'00'00,                 tint16::blackdk },
                    { 0xff'FF'00'00,                 tint16::reddk   },
                    { 0xff'00'00'FF,                 tint16::bluedk  },
                    { 0xff'FF'FF'FF,                 tint16::whitelt },
                    { 0xff'aa'aa'aa,                 tint16::whitedk },
                    { 0xff'80'80'80,                 tint16::graylt  },
                    { 0xff'55'55'55,                 tint16::graydk  },
                });
                return table;
            }();
            return lookup(cache, std::span{ argb::vtm16.data(), 8 });
        }
        // argb: Change endianness to LE.
        friend auto letoh(argb r)
        {
            return argb{ netxs::letoh(r.token) };
        }
    };

    // canvas: Generic RGBA.
    template<class T>
    struct irgb
    {
        T r, g, b, a;

        constexpr irgb() = default;
        constexpr irgb(irgb const&) = default;
        constexpr irgb(T r, T g, T b, T a)
            : r{ r }, g{ g }, b{ b }, a{ a }
        { }
        constexpr irgb(argb c) requires(std::is_integral_v<T>)
            : r{ c.chan.r },
              g{ c.chan.g },
              b{ c.chan.b },
              a{ c.chan.a }
        { }
        constexpr irgb(argb c) requires(std::is_floating_point_v<T>)
            : r{ c.chan.r / 255.f },
              g{ c.chan.g / 255.f },
              b{ c.chan.b / 255.f },
              a{ c.chan.a / 255.f }
        { }

        operator argb() const { return argb{ r, g, b, a }; }

        bool operator > (auto n) const { return r > n || g > n || b > n || a > n; }
        auto operator / (auto n) const { return irgb{ r / n, g / n, b / n, a / n }; } // 10% faster than divround.
        auto operator * (auto n) const { return irgb{ r * n, g * n, b * n, a * n }; }
        auto operator + (irgb const& c) const { return irgb{ r + c.r, g + c.g, b + c.b, a + c.a }; }
        void operator *= (auto n) { r *= n; g *= n; b *= n; a *= n; }
        void operator /= (auto n) { r /= n; g /= n; b /= n; a /= n; }
        void operator =  (irgb const& c) { r =  c.r; g =  c.g; b =  c.b; a =  c.a; }
        void operator += (irgb const& c) { r += c.r; g += c.g; b += c.b; a += c.a; }
        void operator -= (irgb const& c) { r -= c.r; g -= c.g; b -= c.b; a -= c.a; }
        void operator += (argb c) requires(std::is_integral_v<T>) { r += c.chan.r; g += c.chan.g; b += c.chan.b; a += c.chan.a; }
        void operator -= (argb c) requires(std::is_integral_v<T>) { r -= c.chan.r; g -= c.chan.g; b -= c.chan.b; a -= c.chan.a; }
        // irgb: sRGB to Linear (g = 2.4)
        static auto sRGB2Linear(fp32 c)
        {
            return c <= 0.04045f ? c / 12.92f
                                 : std::pow((c + 0.055f) / 1.055f, 2.4f);
        }
        // irgb: Linear to sRGB (g = 2.4)
        static auto linear2sRGB(fp32 c)
        {
            return c <= 0.0031308f ? 12.92f * c
                                   : 1.055f * std::pow(c, 1.f / 2.4f) - 0.055f;
        }
        // irgb: sRGB to linear (g = 2.4)
        irgb& sRGB2Linear() requires(std::is_floating_point_v<T>)
        {
            r = sRGB2Linear(r);
            g = sRGB2Linear(g);
            b = sRGB2Linear(b);
            return *this;
        }
        // irgb: Linear to sRGB (g = 2.4)
        irgb& linear2sRGB() requires(std::is_floating_point_v<T>)
        {
            r = linear2sRGB(r);
            g = linear2sRGB(g);
            b = linear2sRGB(b);
            return *this;
        }
        // irgb: Premultiply alpha (floating point only).
        auto& pma() requires(std::is_floating_point_v<T>)
        {
            if (a != 1.f)
            {
                if (a == 0.f) r = b = g = 0.f;
                else
                {
                    r *= a;
                    g *= a;
                    b *= a;
                }
            }
            return *this;
        }
        // irgb: Blend with pma c (floating point only).
        auto& blend_pma(irgb c) requires(std::is_floating_point_v<T>)
        {
            if (c.a != 0.f)
            {
                if (c.a == 1.f || a == 0.f) *this = c;
                else
                {
                    auto na = 1.f - c.a;
                    r = c.r + na * r;
                    g = c.g + na * g;
                    b = c.b + na * b;
                    a = c.a + na * a;
                }
            }
            return *this;
        }
        // irgb: Blend with non-pma c (0.0-1.0) using integer alpha (0-255).
        auto& blend_nonpma(irgb non_pma_c, byte alpha) requires(std::is_floating_point_v<T>)
        {
            if (alpha == 255) *this = non_pma_c;
            else if (alpha != 0)
            {
                non_pma_c.a *= (T)alpha / 255;
                blend_pma(non_pma_c.pma());
            }
            return *this;
        }
    };

    // canvas: Grapheme cluster.
    struct cell
    {
        struct glyf
        {
            static auto jumbos()
            {
                using lock = std::mutex;
                using sync = std::lock_guard<lock>;
                using depo = std::unordered_map<ui64, text>;
                using uset = std::unordered_set<ui64>;

                struct vars
                {
                    lock mutex{}; // Cluster map mutex. Do we need to reset/clear/flush the map?
                    depo jumbo{}; // Jumbo cluster map.
                    uset undef{}; // List of unknown tokens.
                };
                struct guard : sync
                {
                    depo& map;
                    uset& unk;
                    guard(vars& inst)
                        : sync{ inst.mutex },
                           map{ inst.jumbo },
                           unk{ inst.undef }
                    { }
                    // jumbos: Get cluster.
                    auto& get(ui64 token)
                    {
                        if (auto iter = map.find(token); iter != map.end())
                        {
                            return iter->second;
                        }
                        else
                        {
                            static auto empty = text{};
                            unk.insert(token);
                            return empty;
                        }
                    }
                    // jumbos: Set cluster.
                    void set(ui64 token, view data)
                    {
                        map[token] = data;
                    }
                    // jumbos: Add cluster.
                    void add(ui64 token, view data)
                    {
                        map.insert(std::pair{ token, data }); // Silently ignore if it exists.
                    }
                    // jumbos: Check the cluster existence by token.
                    auto exists(ui64 token)
                    {
                        auto iter = map.find(token);
                        auto okay = iter != map.end();
                        if (!okay) unk.insert(token);
                        return okay;
                    }
                };

                static auto inst = vars{};
                return guard{ inst };
            }

            // If bytes[1] & 0b11'00'0000 == 0b10'00'0000 (first byte in UTF-8 cannot start with 0b10......) - If so, cluster is stored in an external map (jumbo cluster).
            // In Modified UTF-8, the null character (U+0000) uses the two-byte overlong encoding 11000000 10000000 (hexadecimal C0 80), instead of 00000000 (hexadecimal 00).
            // We do not store Nulls, it is used to create cells with an empty string. //todo If necessary, use Jumbo storage to store clusters containing nulls.
            static constexpr auto size_w_mask = netxs::letoh((ui64)0b0000'1111); // 0-based (w - 1) cell matrix width. (w: 1 - 16)  utf::matrix::kx
            static constexpr auto size_h_mask = netxs::letoh((ui64)0b0011'0000); // 0-based (h - 1) cell matrix height. (h: 1 - 4)  utf::matrix::ky
            static constexpr auto is_rtl_mask = netxs::letoh((ui64)0b0100'0000); // Cluster contains RTL text.
            static constexpr auto reserv_mask = netxs::letoh((ui64)0b1000'0000); // Reserved.
            static constexpr auto letoh_shift = netxs::endian_BE ? 8 * 7 : 0; // Left shift to get bytes[0].

            ui64 token;

            constexpr glyf()
                : token{ 0 }
            { }
            constexpr glyf(glyf const& g)
                : token{ g.token }
            { }
            constexpr glyf(char c)
                : token{ netxs::letoh((ui64)c << 8 * 1) } // bytes[1] = c;
            { }

            constexpr glyf& operator = (glyf const&) = default;
            auto operator == (glyf const& g) const
            {
                return token == g.token;
            }

            constexpr auto size_w() const { return (si32)((token & size_w_mask) >> (letoh_shift + 0)); }
            constexpr auto size_h() const { return (si32)((token & size_h_mask) >> (letoh_shift + 4)); }
            constexpr void size_w(si32 w) { token &= ~size_w_mask; token |= (ui64)w << (letoh_shift + 0); }
            constexpr void size_h(si32 h) { token &= ~size_h_mask; token |= (ui64)h << (letoh_shift + 4); }
            constexpr auto rtl()    const { return token & is_rtl_mask; }
            constexpr void rtl(bool b)    { if (b) token |= is_rtl_mask; else token &= ~is_rtl_mask; }
            constexpr auto is_jumbo() const
            {
                return (token & netxs::letoh((ui64)0b1100'0000'0000'0000)) == netxs::letoh((ui64)0b1000'0000'0000'0000); // (bytes[1] & 0b1100'0000) == 0b1000'0000;
            }
            void set_jumbo_flag()
            {
                token = (token & netxs::letoh(~(ui64)0b1100'0000'0000'0000)) | netxs::letoh((ui64)0b1000'0000'0000'0000); // bytes[1] = (bytes[1] & ~0b1100'0000) | 0b1000'0000;// First byte in UTF-8 cannot start with 0b10xx'xxxx.
            }
            auto bytes() const
            {
                return (char*)&token;
            }
            constexpr void set(ui64 t)
            {
                token = netxs::letoh(t);
            }
            constexpr void set(char c)
            {
                auto isrtl = rtl();
                token = netxs::letoh((ui64)c << 8 * 1); // bytes[1] = c;
                token |= isrtl;
            }
            constexpr void set_c0(char c)
            {
                auto isrtl = rtl();
                token = netxs::letoh(((ui64)'^' << 8 * 1) | ((ui64)('@' + (c & 0b00011111)) << 8 * 2)); // bytes[1] = '^'; bytes[2] = '@' + (c & 0b00011111);
                token |= isrtl;
                size_w(2 - 1);
            }
            auto mtx() const
            {
                return twod{ size_w() + 1, size_h() + 1 };
            }
            void mtx(si32 w, si32 h)
            {
                size_w(w ? w - 1 : 0);
                size_h(h ? h - 1 : 0);
            }
            auto jgc_token() const // Return token excluding props.
            {
                auto token_copy = token;
                token_copy &= netxs::letoh(~ui64{} << 8 * 1); // ((char*)&token_copy)[0] &= token_mask; // token_mask = (char)0b0000'0000; // Exclude RTL and matrix metadata.
                return token_copy;
            }
            void set_direct(view utf8, si32 w, si32 h)
            {
                auto count = utf8.size();
                auto isrtl = rtl();
                if (count < sizeof(token))
                {
                    token = isrtl; // token = 0;
                    mtx(w, h);
                    std::memcpy(bytes() + 1, utf8.data(), count);
                }
                else
                {
                    token = qiew::hash{}(utf8);
                    token &= ~is_rtl_mask;
                    token |= isrtl;
                    set_jumbo_flag();
                    mtx(w, h);
                    jumbos().add(jgc_token(), utf8);
                }
            }
            // glyf: Cluster length in bytes (if it is not jumbo).
            auto str_len() const
            {
                return !(token & netxs::letoh((ui64)0xFF << 8 * 1)) ? 0 : // !bytes[1] ? 0 :
                       !(token & netxs::letoh((ui64)0xFF << 8 * 2)) ? 1 : // !bytes[2] ? 1 :
                       !(token & netxs::letoh((ui64)0xFF << 8 * 3)) ? 2 : // !bytes[3] ? 2 :
                       !(token & netxs::letoh((ui64)0xFF << 8 * 4)) ? 3 : // !bytes[4] ? 3 :
                       !(token & netxs::letoh((ui64)0xFF << 8 * 5)) ? 4 : // !bytes[5] ? 4 :
                       !(token & netxs::letoh((ui64)0xFF << 8 * 6)) ? 5 : // !bytes[6] ? 5 :
                       !(token & netxs::letoh((ui64)0xFF << 8 * 7)) ? 6 : 7; // !bytes[7] ? 6 : 7;
            }
            template<svga Mode = svga::vtrgb>
            view get() const
            {
                if constexpr (Mode == svga::dtvt) return {};
                auto crop = is_jumbo() ? jumbos().get(jgc_token())
                                       : view(bytes() + 1, str_len());
                if constexpr (Mode != svga::vt_2D)
                {
                    if (crop.size() && crop.front() == utf::matrix::stx)
                    {
                        crop.remove_prefix(1); // Drop cluster initializer.
                    }
                }
                return crop;
            }
            auto is_space() const
            {
                return (token & netxs::letoh((ui64)0xFF00)) <= netxs::letoh((ui64)whitespace << 8); // (byte)(bytes[1]) <= whitespace;
            }
            auto is_null() const
            {
                return (token & netxs::letoh((ui64)0xFF00)) == 0; // bytes[1] == 0; // Jumbo bits are nulls. Jumbo mark is the last two bits = 0b10'000000.
            }
            auto jgc() const
            {
                return !is_jumbo() || jumbos().exists(jgc_token());
            }
            // Return cluster storage length.
            auto len() const
            {
                return is_jumbo() ? sizeof(token) : 1/*first byte (props)*/ + str_len();
            }
            void rst()
            {
                set(whitespace);
            }
            void wipe()
            {
                token = 0;
            }
        };
        struct body
        {
            struct pxtype
            {
                static constexpr auto none   = 0;
                static constexpr auto bitmap = 1; // Attached argb bitmap reference: 32 bit: bitmap index.
                static constexpr auto reserv = 2;
            };

            // Shared attributes.
            static constexpr auto bolded_mask = (ui64)0b00000000'00000000'00000000'00000000'00000000'00000000'00000000'00000001; // bolded : 1;
            static constexpr auto italic_mask = (ui64)0b00000000'00000000'00000000'00000000'00000000'00000000'00000000'00000010; // italic : 1;
            static constexpr auto invert_mask = (ui64)0b00000000'00000000'00000000'00000000'00000000'00000000'00000000'00000100; // invert : 1;
            static constexpr auto overln_mask = (ui64)0b00000000'00000000'00000000'00000000'00000000'00000000'00000000'00001000; // overln : 1;
            static constexpr auto strike_mask = (ui64)0b00000000'00000000'00000000'00000000'00000000'00000000'00000000'00010000; // strike : 1;
            static constexpr auto unline_mask = (ui64)0b00000000'00000000'00000000'00000000'00000000'00000000'00000000'11100000; // unline : 3; // 0: none, 1: line, 2: biline, 3: wavy, 4: dotted, 5: dashed, 6 - 7: unknown.
            static constexpr auto ucolor_mask = (ui64)0b00000000'00000000'00000000'00000000'00000000'00000000'11111111'00000000; // ucolor : 8; // Underline 256-color 6x6x6-cube index. Alpha not used - it is shared with fgc alpha. If zero - sync with fgc.
            static constexpr auto cursor_mask = (ui64)0b00000000'00000000'00000000'00000000'00000000'00000011'00000000'00000000; // cursor : 2; // 0: None, 1: Underline, 2: Block, 3: I-bar.
            static constexpr auto hplink_mask = (ui64)0b00000000'00000000'00000000'00000000'00000000'00000100'00000000'00000000; // hyperlink : 1; // cell::px strores string hash.
            static constexpr auto blinks_mask = (ui64)0b00000000'00000000'00000000'00000000'00000000'00001000'00000000'00000000; // blinks : 1;
            static constexpr auto bitmap_mask = (ui64)0b00000000'00000000'00000000'00000000'00000000'00110000'00000000'00000000; // bitmap : 2; // body::pxtype: Cursor losts its colors when it covers bitmap.
            static constexpr auto fusion_mask = (ui64)0b00000000'00000000'00000000'00000000'00000000'11000000'00000000'00000000; // fusion : 2; // todo The outlines of object boundaries must be set when rendering each window (pro::shape).
            static constexpr auto shadow_mask = (ui64)0b00000000'00000000'00000000'00000000'11111111'00000000'00000000'00000000; // shadow : 8; // Shadow bits.
            static constexpr auto hidden_mask = (ui64)0b00000000'00000000'00000000'00000001'00000000'00000000'00000000'00000000; // shadow : 8; // Hidden character.
            // Unique attributes. From 24th bit.
            static constexpr auto mosaic_mask = (ui64)0b00000000'00000000'11111111'00000000'00000000'00000000'00000000'00000000; // ui32 mosaic : 8; // High 3 bits -> y-fragment (0-4 utf::matrix::ky), low 5 bits -> x-fragment (0-16 utf::matrix::kx). // Ref:  https://gitlab.freedesktop.org/terminal-wg/specifications/-/issues/23
            static constexpr auto curbgc_mask = (ui64)0b00000000'11111111'00000000'00000000'00000000'00000000'00000000'00000000; // bgcclr : 8; // Cursor 256-color 6x6x6-cube index. Alpha not used.
            static constexpr auto curfgc_mask = (ui64)0b11111111'00000000'00000000'00000000'00000000'00000000'00000000'00000000; // fgcclr : 8; // Cursor 256-color 6x6x6-cube index. Alpha not used.

            static constexpr auto x_bits = utf::matrix::x_bits; // Character geometry x fragment selector bits (for mosaic_mask).
            static constexpr auto y_bits = utf::matrix::y_bits; // Character geometry y fragment selector bits offset (for mosaic_mask).
            static constexpr auto shared_bits = ((ui64)1 << netxs::field_offset<mosaic_mask>()) - 1;

            // Fusion: Background interpolation current c0 with neighbor c1 and c2 cells:
            //    c0 c1
            //    c2
            // BG interpolation type (two 1-bit values):
            // 0 -- None
            // 1 -- Cubic
            //
            // 0 1
            // │ └────── interpolation type between `c0` and `c2`
            // └──────── interpolation type between `c0` and `c1`

            //todo Cf's can not be entered: even using paste from clipboard
            //don't show (drop) Cf's but allow input it in any order (app is responsible to show it somehow)

            //todo application context: word delimeters (use it in a word/line wrapping, check the last codepoint != Cf | Spc):
            // append prev: U+200C ZERO WIDTH NON-JOINER
            // append prev: U+00AD SOFT HYPHEN

            ui64 token;

            constexpr body()
                : token{ }
            { }
            constexpr body(body const& b)
                : token{ b.token }
            { }
            constexpr body(si32 mosaic)
                : token{ (ui64)(ui32)mosaic << netxs::field_offset<mosaic_mask>() }
            { }
            constexpr body(body const& b, si32 mosaic)
                : token{ (b.token & ~mosaic_mask) | ((ui64)(ui32)mosaic << netxs::field_offset<mosaic_mask>()) }
            { }

            constexpr body& operator = (body const&) = default;
            bool operator == (body const& b) const
            {
                return token == b.token;
            }
            bool operator != (body const& b) const
            {
                return !operator==(b);
            }
            bool like(body const& b) const
            {
                return (token & body::shared_bits) == (b.token & body::shared_bits);
            }
            void meta(body const& b)
            {
                token = (token & body::mosaic_mask) | (b.token & ~body::mosaic_mask); // Keep mosaic.
            }
            void meta_shadow(body const& b)
            {
                token = (token & (body::mosaic_mask | body::shadow_mask)) | (b.token & ~body::mosaic_mask); // Keep mosaic and OR'ing shadow.
            }
            void meta_shadow_matrix(body const& b)
            {
                token = (token & body::shadow_mask) | b.token; // Update meta with OR'ing shadow.
            }
            template<svga Mode = svga::vtrgb, bool UseSGR = true, class T>
            void get(body& base, T& dest) const
            {
                if constexpr (Mode == svga::dtvt) return;
                if (!like(base))
                {
                    if constexpr (UseSGR) // It is not available in the Linux and Win8 consoles.
                    {
                        if constexpr (Mode == svga::vt_2D)
                        {
                            if (auto cursor = token & cursor_mask; cursor != (base.token & cursor_mask)) dest.cursor0((si32)(cursor >> netxs::field_offset<cursor_mask>()));
                            if (auto shadow = token & shadow_mask; shadow != (base.token & shadow_mask)) dest.dim(    (si32)(shadow >> netxs::field_offset<shadow_mask>()));
                            //if (auto hplink = token & hplink_mask; hplink != (base.token & hplink_mask)) dest.hplink0((si32)(hplink >> netxs::field_offset<hplink_mask>()));
                            //if (auto fusion = token & fusion_mask; fusion != (base.token & fusion_mask)) dest.fusion0((si32)(fusion >> netxs::field_offset<fusion_mask>()));
                            //todo sync px
                            //if (auto bitmap = token & bitmap_mask; bitmap != (base.token & bitmap_mask)) dest.bitmap0(!!bitmap);
                        }
                        if constexpr (Mode != svga::vt16) // It is not available in the Linux and Win8 consoles.
                        {
                            if (auto bolded = token & bolded_mask; bolded != (base.token & bolded_mask)) dest.bld(!!bolded);
                            if (auto italic = token & italic_mask; italic != (base.token & italic_mask)) dest.itc(!!italic);
                            if (auto invert = token & invert_mask; invert != (base.token & invert_mask)) dest.inv(!!invert);
                            if (auto overln = token & overln_mask; overln != (base.token & overln_mask)) dest.ovr(!!overln);
                            if (auto strike = token & strike_mask; strike != (base.token & strike_mask)) dest.stk(!!strike);
                            if (auto blinks = token & blinks_mask; blinks != (base.token & blinks_mask)) dest.blk(!!blinks);
                            if (auto hidden = token & hidden_mask; hidden != (base.token & hidden_mask)) dest.hid(!!hidden);
                            if (auto unline = token & unline_mask; unline != (base.token & unline_mask)) dest.und((si32)(unline >> netxs::field_offset<unline_mask>()));
                            if (auto ucolor = token & ucolor_mask; ucolor != (base.token & ucolor_mask)) dest.unc((si32)(ucolor >> netxs::field_offset<ucolor_mask>()));
                        }
                        else
                        {
                            if (auto unline = token & unline_mask; unline != (base.token & unline_mask)) dest.inv((si32)(unline >> netxs::field_offset<unline_mask>()));
                        }
                    }
                    base.token = token;
                }
            }
            void wipe()
            {
                token = 0;
            }
            void reverse()
            {
                token ^= invert_mask;
            }

            void bld(bool b)         { token &= ~bolded_mask; token |= ((ui64)b << netxs::field_offset<bolded_mask>()); }
            void itc(bool b)         { token &= ~italic_mask; token |= ((ui64)b << netxs::field_offset<italic_mask>()); }
            void inv(bool b)         { token &= ~invert_mask; token |= ((ui64)b << netxs::field_offset<invert_mask>()); }
            void ovr(bool b)         { token &= ~overln_mask; token |= ((ui64)b << netxs::field_offset<overln_mask>()); }
            void stk(bool b)         { token &= ~strike_mask; token |= ((ui64)b << netxs::field_offset<strike_mask>()); }
            void blk(bool b)         { token &= ~blinks_mask; token |= ((ui64)b << netxs::field_offset<blinks_mask>()); }
            void hid(bool b)         { token &= ~hidden_mask; token |= ((ui64)b << netxs::field_offset<hidden_mask>()); }
            void dim(si32 n)         { token &= ~shadow_mask; token |= ((ui64)(ui32)n << netxs::field_offset<shadow_mask>()); }
            void und(si32 n)         { token &= ~unline_mask; token |= ((ui64)(ui32)n << netxs::field_offset<unline_mask>()); }
            void unc(si32 c)         { token &= ~ucolor_mask; token |= ((ui64)(ui32)c << netxs::field_offset<ucolor_mask>()); }
            void cur(si32 s)         { token &= ~cursor_mask; token |= ((ui64)(ui32)s << netxs::field_offset<cursor_mask>()); }
            void mosaic(si32 m)      { token &= ~mosaic_mask; token |= ((ui64)(ui32)m << netxs::field_offset<mosaic_mask>()); }
            void bitmap(si32 r)      { token &= ~bitmap_mask; token |= ((ui64)(ui32)r << netxs::field_offset<bitmap_mask>()); }
            void  xy(ui64 m)         { token &= ~mosaic_mask; token |= m; }
            void raw(ui64 r)         { token &= ~bitmap_mask; token |= r; }
            void  xy(si32 x, si32 y) { mosaic(x + (y << y_bits)); }
            void fuse_dim(si32 n)    { token |= (ui64)(ui32)n << netxs::field_offset<shadow_mask>(); }
            void cursor0(si32 c)     { token &= ~cursor_mask; token |= ((ui64)(ui32)c << netxs::field_offset<cursor_mask>()); }
            void cursor_color(argb bgc, argb fgc)
            {
                auto bg = bgc.to_256cube();
                auto fg = fgc.to_256cube();
                token &= ~(curbgc_mask | curfgc_mask);
                token |= ((ui64)bg << netxs::field_offset<curbgc_mask>());
                token |= ((ui64)fg << netxs::field_offset<curfgc_mask>());
            }
            //void hplink0(ui64 c) { token &= ~hplink_mask; token |= (ui64)(c << netxs::field_offset<hplink_mask>()); }
            //void fusion0(ui64 c) { token &= ~fusion_mask; token |= (ui64)(c << netxs::field_offset<fusion_mask>()); }

            bool bld()    const { return !!(token & bolded_mask); }
            bool itc()    const { return !!(token & italic_mask); }
            bool inv()    const { return !!(token & invert_mask); }
            bool ovr()    const { return !!(token & overln_mask); }
            bool stk()    const { return !!(token & strike_mask); }
            bool blk()    const { return !!(token & blinks_mask); }
            bool hid()    const { return !!(token & hidden_mask); }
            si32 und()    const { return (si32)((token & unline_mask) >> netxs::field_offset<unline_mask>()); }
            si32 dim()    const { return (si32)((token & shadow_mask) >> netxs::field_offset<shadow_mask>()); }
            si32 unc()    const { return (si32)((token & ucolor_mask) >> netxs::field_offset<ucolor_mask>()); }
            si32 cur()    const { return (si32)((token & cursor_mask) >> netxs::field_offset<cursor_mask>()); }
            //si32 cursor0() const { return (token & cursor_mask); }
            //si32 hplink0() const { return (token & hplink_mask); }
            //si32 fusion0() const { return (token & fusion_mask); }
            ui64  xy()    const { return (token & mosaic_mask); }
            ui64 raw()    const { return (token & bitmap_mask); }
            si32 mosaic() const { return (si32)((token & mosaic_mask) >> netxs::field_offset<mosaic_mask>()); }
            si32 bitmap() const { return (si32)((token & bitmap_mask) >> netxs::field_offset<bitmap_mask>()); }
            auto cursor_color() const
            {
                auto bgi = (byte)((token & curbgc_mask) >> netxs::field_offset<curbgc_mask>());
                auto fgi = (byte)((token & curfgc_mask) >> netxs::field_offset<curfgc_mask>());
                auto bgc = bgi ? argb{ argb::vt256[bgi] } : argb{};
                auto fgc = fgi ? argb{ argb::vt256[fgi] } : argb{};
                return std::pair{ bgc, fgc };
            }
        };
        struct clrs
        {
            argb bg;
            argb fg;

            constexpr clrs() = default;
            constexpr clrs(auto colors)
                : bg{ *(colors.begin() + 0) },
                  fg{ *(colors.begin() + 1) }
            { }
            constexpr clrs(clrs const& c)
                : bg{ c.bg },
                  fg{ c.fg }
            { }

            constexpr clrs& operator = (clrs const&) = default;
            constexpr bool operator == (clrs const& c) const
            {
                return bg == c.bg
                    && fg == c.fg;
                // sizeof(*this);
            }
            constexpr bool operator != (clrs const& c) const
            {
                return !operator==(c);
            }

            static void fix_collision_vga16(auto& f) // Fix color collision in low-color mode.
            {
                assert(f < 16);
                if (f <= tint::whitedk) f += 8;
                else                    f -= 8;
            }
            static void fix_collision_vtm16(auto& f) // Fix color collision in low-color mode.
            {
                assert(f < 16);
                     if (f  < tint16::whitelt ) f += 1;
                else if (f == tint16::whitelt ) f -= 1;
                else if (f <= tint16::yellowdk) f += 6; // Make it lighter.
                else if (f <= tint16::cyanlt  ) f = tint16::graylt;
                else if (f <= tint16::yellowlt) f -= 6; // Make it darker.
            }
            static void fix_collision_vtm8(auto& f) // Fix color collision in low-color mode.
            {
                assert(f < 8);
                     if (f  < tint16::whitelt) f += 1;
                else if (f == tint16::whitelt) f -= 1;
                else                           f = tint16::whitedk;
            }
            template<svga Mode = svga::vtrgb, bool UseSGR = true, class T>
            void get(clrs& base, T& dest) const
            {
                if constexpr (Mode == svga::dtvt) return;
                if constexpr (Mode == svga::vt16)
                {
                    if (fg != base.fg || bg != base.bg)
                    {
                        if constexpr (UseSGR)
                        {
                            auto f = fg.to_vtm16();
                            auto b = bg.to_vtm8();
                            if (fg != bg && f == b) // Avoid color collizions.
                            {
                                fix_collision_vtm8(f);
                                if (bg != base.bg) dest.bgc_8(b);
                                dest.fgc_16(f);
                            }
                            else
                            {
                                if (bg != base.bg) dest.bgc_8(b);
                                if (fg != base.fg) dest.fgc_16(f);
                            }
                        }
                        base.bg = bg;
                        base.fg = fg;
                    }
                }
                else
                {
                    if (bg != base.bg)
                    {
                        base.bg = bg;
                        if constexpr (UseSGR) dest.template bgc<Mode>(bg);
                    }
                    if (fg != base.fg)
                    {
                        base.fg = fg;
                        if constexpr (UseSGR) dest.template fgc<Mode>(fg);
                    }
                }
            }
            void wipe()
            {
                bg.wipe();
                fg.wipe();
            }
        };
        struct pict
        {
            ui32 token;
            constexpr pict()
                : token{ 0 }
            { }
            constexpr pict(pict const& p)
                : token{ p.token }
            { }

            constexpr pict& operator = (pict const&) = default;
            bool operator == (pict const& p) const
            {
                return token == p.token;
            }
            void wipe()
            {
                token = 0;
            }
        };

        clrs uv; // 8U, cell: Fg and bg colors.
        glyf gc; // 8U, cell: Grapheme cluster.
        body st; // 8U, cell: Style attributes.
        id_t id; // 4U, cell: Link ID.
        pict px; // 4U, cell: Reference to the raw bitmap attached to the cell.

        cell()
            : id{ 0 }
        { }
        cell(char c)
            : gc{ c },
              st{ utf::matrix::mosaic<11> },
              id{ 0 }
        {
            // sizeof(glyf);
            // sizeof(clrs);
            // sizeof(body);
            // sizeof(id_t);
            // sizeof(pict);
            // sizeof(cell);
        }
        cell(view utf8)
            : id{ 0 }
        {
            txt(utf8);
        }
        cell(cell const& base)
            : uv{ base.uv },
              gc{ base.gc },
              st{ base.st },
              id{ base.id },
              px{ base.px }
        { }
        cell(cell const& base, char c)
            : uv{ base.uv },
              gc{ c       },
              st{ base.st, utf::matrix::mosaic<11> },
              id{ base.id },
              px{ base.px }
        { }

        auto operator == (cell const& c) const
        {
            return uv == c.uv
                && st == c.st
                && gc == c.gc
                && px == c.px;
        }
        auto operator != (cell const& c) const
        {
            return !operator==(c);
        }
        auto& operator = (cell const& c)
        {
            uv = c.uv;
            gc = c.gc;
            st = c.st;
            id = c.id;
            px = c.px;
            return *this;
        }

        operator bool () const { return st.xy(); } // cell: Return true if cell contains printable character.

        auto is_empty() const // cell: Return true if cell is absolutely empty.
        {
            return uv.bg.token == 0 && uv.fg.token == 0 && gc.token == 0 && st.token == 0 && id == 0 && px.token == 0;
        }
        auto same_txt(cell const& c) const // cell: Compare clusters.
        {
            return gc == c.gc;
        }
        bool like(cell const& c) const // cell: Meta comparison of the two cells.
        {
            return uv == c.uv
                && st.like(c.st)
                && (!st.raw() || px == c.px);
        }
        void wipe() // cell: Set colors, attributes and grapheme cluster to zero.
        {
            uv.wipe();
            gc.wipe();
            st.wipe();
            px.wipe();
        }
        // cell: Blend two cells according to visibility and other attributes.
        auto& fuse(cell const& c)
        {
            if (uv.fg.chan.a == 0xFF) uv.fg.mix_one(c.uv.fg);
            else                      uv.fg.mix(c.uv.fg);

            if (uv.bg.chan.a == 0xFF) uv.bg.mix_one(c.uv.bg);
            else                      uv.bg.mix(c.uv.bg);

            if (auto r = c.st.raw())
            {
                px = c.px;
                st.raw(r);
            }
            if (c.st.xy())
            {
                gc = c.gc;
                if (c.uv.bg.token == 0) // OR'ing the shadow if bg is completely transparent.
                {
                    st.meta_shadow_matrix(c.st);
                }
                else
                {
                    st = c.st;
                }
            }
            else
            {
                if (c.uv.bg.token == 0) // OR'ing the shadow if bg is completely transparent.
                {
                    st.meta_shadow(c.st);
                }
                else
                {
                    st.meta(c.st);
                }
            }
            return *this;
        }
        // cell: Blend two cells if text part != '\0'.
        inline void lite(cell const& c)
        {
            if (!c.gc.is_null()) fuse(c); // if (c.gc.bytes[1] != 0) fuse(c);
        }
        // cell: Blend cell colors.
        void mix(cell const& c)
        {
            uv.fg.mix_one(c.uv.fg);
            uv.bg.mix_one(c.uv.bg);
            if (c.st.xy())
            {
                st = c.st;
                gc = c.gc;
            }
            if (st.raw()) px = c.px;
        }
        // cell: Blend cell colors.
        void blend(cell const& c)
        {
            uv.fg.mix(c.uv.fg);
            uv.bg.mix(c.uv.bg);
        }
        // cell: Blend colors using alpha.
        void mix(cell const& c, byte alpha)
        {
            uv.fg.mix(c.uv.fg, alpha);
            uv.bg.mix(c.uv.bg, alpha);
            if (auto r = c.st.raw())
            {
                px = c.px;
                st.raw(r);
            }
            if (c.st.xy())
            {
                st = c.st;
                gc = c.gc;
            }
        }
        // cell: Blend colors using alpha.
        void mixfull(cell const& c, si32 alpha)
        {
            if (c.id) id = c.id;
            if (c.st.xy())
            {
                st = c.st;
                gc = c.gc;
                uv.fg = uv.bg; // The character must be on top of the cell background. (see block graphics)
            }
            if (st.raw()) px = c.px;
            uv.fg.mix(c.uv.fg, alpha);
            uv.bg.mix(c.uv.bg, alpha);
        }
        // cell: Blend two cells and set specified id.
        void fuse(cell const& c, id_t oid)
        {
            fuse(c);
            id = oid;
        }
        // cell: Blend two cells and set id if it is.
        void fusefull(cell const& c)
        {
            fuse(c);
            if (c.id) id = c.id;
        }
        // cell: Blend two cells and set id if it is (fg = bg * c.fg).
        void overlay(cell const& c)
        {
            auto bg_opaque = uv.bg.chan.a == 0xFF;
            if (c.st.xy() || c.st.und())
            {
                uv.fg = uv.bg;
                if (bg_opaque) uv.fg.mix_one(c.uv.fg);
                else           uv.fg.mix(c.uv.fg);
            }
            else
            {
                if (uv.fg.chan.a == 0xFF) uv.fg.mix_one(c.uv.bg);
                else                      uv.fg.mix(c.uv.bg);
            }
            gc = c.gc;
            st = c.st;
            if (bg_opaque) uv.bg.mix_one(c.uv.bg);
            else           uv.bg.mix(c.uv.bg);
            if (c.st.raw())
            {
                px = c.px;
            }
            if (c.id) id = c.id;
        }
        // cell: Merge two cells and set id.
        void fuseid(cell const& c)
        {
            fuse(c);
            id = c.id;
        }
        void meta(cell const& c)
        {
            uv = c.uv;
            st.meta(c.st);
            px = c.px;
        }
        void skipnulls(cell const& c)
        {
            if (c.gc.is_null()) // Keep gc intact.
            {
                if (c.uv.bg.token != argb::default_color) // Completely ignore transparent nulls (do nothing, move cursor forward).
                {
                    meta(c);
                }
            }
            else
            {
                if (c.uv.bg.token == argb::default_color) // Update gc while keeping SGR attributes (if bgc==0x00'FF'FF'FF).
                {
                    gc = c.gc;
                    st.xy(c.st.xy());
                }
                else // Copy all.
                {
                    *this = c;
                }
            }
        }
        // cell: Get differences of the visual attributes only (ANSI CSI/SGR format).
        template<svga Mode = svga::vtrgb, bool UseSGR = true, class T>
        void scan_attr(cell& base, T& dest) const
        {
            if (!like(base))
            {
                //todo additionally consider UNIQUE ATTRIBUTES
                uv.get<Mode, UseSGR>(base.uv, dest);
                st.get<Mode, UseSGR>(base.st, dest);
                //todo raw bitmap
            }
        }
        // cell: Render colored whitespaces instead of "░▒▓".
        template<svga Mode = svga::vtrgb, bool UseSGR = true, class T>
        void filter(cell& base, T& dest) const
        {
            if constexpr (UseSGR && (Mode == svga::vtrgb || Mode == svga::vt_2D))
            {
                auto egc = gc.get<Mode>();
                if (egc.size() == 3 && egc[0] == '\xE2' && egc[1] == '\x96')
                {
                    auto k = 0;
                         if (egc[2] == '\x91') k = 64;  // "░"
                    else if (egc[2] == '\x92') k = 96;  // "▒"
                    else if (egc[2] == '\x93') k = 128; // "▓"
                    else
                    {
                        dest += egc;
                        return;
                    }
                    auto bgc = argb::transit(base.uv.bg, base.uv.fg, k);
                    if (bgc != base.uv.bg)
                    {
                        base.uv.bg = bgc;
                        dest.template bgc<Mode>(bgc);
                    }
                    dest += whitespace;
                }
                else dest += egc;
            }
            else dest += gc.get<Mode>();
        }
        // cell: Get differences (ANSI CSI/SGR format) of "base" and add it to "dest" and update the "base".
        template<svga Mode = svga::vtrgb, bool UseSGR = true, class T>
        void scan(cell& base, T& dest) const
        {
            if constexpr (Mode != svga::dtvt)
            {
                if (!like(base))
                {
                    //todo additionally consider UNIQUE ATTRIBUTES
                    uv.get<Mode, UseSGR>(base.uv, dest);
                    st.get<Mode, UseSGR>(base.st, dest);
                    //todo raw bitmap
                }
                if (st.xy() && !gc.is_space()) filter<Mode, UseSGR>(base, dest);
                else                           dest += whitespace;
                //if (st.xy())
                //{
                //    filter<Mode, UseSGR>(base, dest);
                //}
                //else // Allow nulls to be copyable.
                //{
                //    auto cluster = gc.get<Mode>();
                //    if (cluster.size()) dest += cluster;
                //    else                dest += emptyspace;
                //}
            }
        }
        // cell: Check that the halves belong to the same wide glyph.
        bool check_pair(cell const& next) const
        {
            return gc == next.gc && like(next);
        }
        // cell: Return cluster matrix metadata.
        auto whxy() const  { return std::tuple{ (si32)(gc.size_w() + 1),
                                                (si32)(gc.size_h() + 1),
                                                (si32)(st.mosaic() & cell::body::x_bits),
                                                (si32)(st.mosaic() >> cell::body::y_bits) }; }
        // cell: Return true if cell is at the matrix right border.
        auto matrix_end() const
        {
            auto w = gc.size_w() + 1;
            return w > 1 && w == /*x*/(st.mosaic() & cell::body::x_bits);
        }
        // cell: Convert to text. Ignore right half. Convert binary clusters (eg: ^C -> 0x03).
        void scan(text& dest) const
        {
            auto [w, h, x, y] = whxy();
            if (w == 0 || h != 1 || x != 1) dest += whitespace;
            else
            {
                auto shadow = gc.get();
                if (shadow.size() == 2 && shadow.front() == '^')
                {
                    dest += shadow[1] & (' ' - 1);
                }
                else dest += shadow;
            }
        }
        // cell: Convert non-printable chars to escaped.
        template<class C>
        auto& c0_to_txt(C chr)
        {
            auto c = static_cast<char>(chr);
            if (c < ' ') gc.set_c0(c);
            return *this;
        }
        // cell: Highlight both foreground and background.
        auto& xlight(si32 factor = 1)
        {
            uv.bg.xlight(factor, uv.fg);
            return *this;
        }
        // cell: Highlight by underlining.
        auto& underlight(si32 factor = 1)
        {
            auto fgc = uv.fg;
            auto bgc = uv.bg;
            if (st.inv()) std::swap(fgc, bgc);
            auto index = st.unc();
            auto color = st.und() == unln::line ? index ? argb{ argb::vt256[index] }.alpha(fgc.alpha()) : fgc
                                                : bgc;
            color.xlight(factor);
            st.unc(color.to_256cube());
            st.und(unln::line);
            return *this;
        }
        // cell: Invert both foreground and background.
        void invert()
        {
            uv.fg.invert();
            uv.bg.invert();
        }
        // cell: Swap foreground and background.
        void reverse()
        {
            std::swap(uv.fg, uv.bg);
        }
        // cell: Flip inversion bit.
        void invbit()
        {
            st.reverse();
        }
        // cell: Desaturate and dim fg color.
        void disabled()
        {
            uv.fg.grayscale();
            uv.fg.shadow(78);
            uv.fg.chan.a = 0xff;
        }
        auto& dim(si32 n)
        {
            if (n == -1)
            {
                uv.fg.faint();
            }
            else
            {
                st.dim(std::clamp(n, 0, 255));
            }
            return *this;
        }
        // cell: Is the cell not transparent?
        bool is_alpha_blendable() const
        {
            return uv.bg.is_alpha_blendable();//&& uv.param.fg.is_alpha_blendable();
        }
        // cell: Cell transitional color blending (fg/bg only).
        void avg(cell const& c1, cell const& c2, si32 level)
        {
            uv.fg = argb::transit(c1.uv.fg, c2.uv.fg, level);
            uv.bg = argb::transit(c1.uv.bg, c2.uv.bg, level);
        }
        // cell: Set grapheme cluster.
        void set_gc(cell const& c)
        {
            gc = c.gc;
            st.xy(c.st.xy());
        }
        // cell: Same grapheme cluster fragment.
        auto same_fragment(cell const& c) const
        {
            return gc == c.gc && st.xy() == c.st.xy();
        }
        // cell: Reset grapheme cluster.
        void set_gc()
        {
            gc.wipe();
            st.xy(0);
        }
        // cell: Copy view of the cell (preserve ID).
        auto& set(cell const& c) { uv = c.uv;
                                   st = c.st;
                                   gc = c.gc;
                                   px = c.px;              return *this; }
        auto& bgc(argb c)        { uv.bg = c;              return *this; } // cell: Set background color.
        auto& fgc(argb c)        { uv.fg = c;              return *this; } // cell: Set foreground color.
        auto& bga(si32 k)        { uv.bg.chan.a = (byte)k; return *this; } // cell: Set background alpha/transparency.
        auto& fga(si32 k)        { uv.fg.chan.a = (byte)k; return *this; } // cell: Set foreground alpha/transparency.
        auto& alpha(si32 k)      { uv.bg.chan.a = (byte)k;
                                   uv.fg.chan.a = (byte)k; return *this; } // cell: Set alpha/transparency (background and foreground).
        // cell: Set/Reset bold attribute. //todo ? SGR22: If b=faux and st.bld()=faux then un-dim fg color.
        auto& bld(bool b)
        {
            //if (st.bld() == faux && b == faux) // Un-dim fg color.
            //{
            //    uv.fg.bright(2);
            //}
            st.bld(b);
            return *this;
        }
        auto& itc(bool b)        { st.itc(b);              return *this; } // cell: Set italic attribute.
        auto& und(si32 n)        { st.und(n);              return *this; } // cell: Set underline attribute.
        auto& unc(argb c)        { st.unc(c.to_256cube()); return *this; } // cell: Set underline color.
        auto& unc(si32 c)        { st.unc(c);              return *this; } // cell: Set underline color.
        auto& cur(si32 s)        { st.cur(s);              return *this; } // cell: Set cursor style.
        auto& img(ui32 p)        { px.token = p;           return *this; } // cell: Set attached bitmap.
        auto& ovr(bool b)        { st.ovr(b);              return *this; } // cell: Set overline attribute.
        auto& inv(bool b)        { st.inv(b);              return *this; } // cell: Set invert attribute.
        auto& stk(bool b)        { st.stk(b);              return *this; } // cell: Set strikethrough attribute.
        auto& blk(bool b)        { st.blk(b);              return *this; } // cell: Set blink attribute.
        auto& hid(bool b)        { st.hid(b);              return *this; } // cell: Set hidden attribute.
        auto& rtl(bool b)        { gc.rtl(b);              return *this; } // cell: Set RTL attribute.
        auto& mtx(twod p)        { gc.mtx(p.x, p.y);       return *this; } // cell: Set glyph matrix.
        auto& xy(si32 x, si32 y) { st.xy(x, y);            return *this; } // cell: Set glyph fragment.
        auto& link(id_t oid)     { id = oid;               return *this; } // cell: Set object ID.
        auto& cursor0(si32 i)    { st.cursor0(i);          return *this; } // cell: Set cursor inside the cell.
        auto& link(cell const& c){ id = c.id;              return *this; } // cell: Set object ID.
        // cell: Set cluster unidata width.
        auto& wdt(si32 vs)
        {
            auto [w, h, x, y] = utf::matrix::whxy(vs);
            gc.mtx(w, h);
            st.xy(x, y);
            return *this;
        }
        auto& wdt(si32 w, si32 h, si32 x, si32 y)
        {
            gc.mtx(w, h);
            st.xy(x, y);
            return *this;
        }
        auto& txt(view utf8, si32 vs)
        {
            auto [w, h, x, y] = utf::matrix::whxy(vs);
            gc.set_direct(utf8, w, h);
            st.xy(x, y);
            return *this;
        }
        auto& txt(view utf8, si32 w, si32 h, si32 x, si32 y)
        {
            gc.set_direct(utf8, w, h);
            st.xy(x, y);
            return *this;
        }
        cell& txt(view utf8)
        {
            if (utf8.empty())
            {
                gc.token = 0;
                st.xy(0);
            }
            else
            {
                auto cluster = utf::cluster(utf8);
                auto [w, h, x, y] = utf::matrix::whxy(cluster.attr.cmatrix);
                gc.set_direct(cluster.text, w, h);
                st.xy(x, y);
            }
            return *this;
        }
        cell& txt2(view utf8, si32 vs)
        {
            auto [w, h, x, y] = utf::matrix::whxy(vs);
            gc.set_direct(utf8, w, h);
            st.xy(x, y);
            return *this;
        }
        auto& txt(char c)        { gc.set(c); st.mosaic(utf::matrix::mosaic<11>);   return *this; } // cell: Set grapheme cluster from char.
        auto& txt(cell const& c) { gc = c.gc;              return *this; } // cell: Set grapheme cluster from cell.
        auto& clr(cell const& c) { uv = c.uv;              return *this; } // cell: Set the foreground and background colors only.
        auto& rst() // cell: Reset view attributes of the cell to zero.
        {
            static auto empty = cell{ whitespace };
            uv = empty.uv;
            st = empty.st;
            gc = empty.gc;
            px = empty.px;
            return *this;
        }

        auto  rtl() const  { return gc.rtl();      } // cell: Return RTL attribute.
        auto  mtx() const  { return gc.mtx();      } // cell: Return cluster matrix size (in cells).
        auto  len() const  { return gc.len();      } // cell: Return grapheme cluster cell storage length (in bytes).
        auto  tkn() const  { return gc.token;      } // cell: Return grapheme cluster token.
        bool  jgc() const  { return gc.jgc();      } // cell: Check the grapheme cluster registration (foreign jumbo clusters).
        ui64   xy() const  { return st.xy();       } // cell: Return matrix fragment metadata.
        template<svga Mode = svga::vtrgb>
        auto  txt() const  { return gc.get<Mode>(); } // cell: Return grapheme cluster.
        auto& egc()        { return gc;            } // cell: Get grapheme cluster object.
        auto& egc() const  { return gc;            } // cell: Get grapheme cluster object.
        auto  clr() const  { return uv.bg || uv.fg;} // cell: Return true if color set.
        auto  bga() const  { return uv.bg.chan.a;  } // cell: Return background alpha/transparency.
        auto  fga() const  { return uv.fg.chan.a;  } // cell: Return foreground alpha/transparency.
        auto& bgc()        { return uv.bg;         } // cell: Return background color.
        auto& fgc()        { return uv.fg;         } // cell: Return foreground color.
        auto& bgc() const  { return uv.bg;         } // cell: Return background color.
        auto& fgc() const  { return uv.fg;         } // cell: Return foreground color.
        auto  bld() const  { return st.bld();      } // cell: Return bold attribute.
        auto  itc() const  { return st.itc();      } // cell: Return italic attribute.
        auto  und() const  { return st.und();      } // cell: Return underline/Underscore attribute.
        auto  unc() const  { return st.unc();      } // cell: Return underline color.
        auto  cur() const  { return st.cur();      } // cell: Return cursor style.
        auto& img()        { return px.token;      } // cell: Return attached bitmap.
        auto& img() const  { return px.token;      } // cell: Return attached bitmap.
        auto  ovr() const  { return st.ovr();      } // cell: Return overline attribute.
        auto  inv() const  { return st.inv();      } // cell: Return negative attribute.
        auto  stk() const  { return st.stk();      } // cell: Return strikethrough attribute.
        auto  blk() const  { return st.blk();      } // cell: Return blink attribute.
        auto  hid() const  { return st.hid();      } // cell: Return hidden attribute.
        auto  dim() const  { return st.dim();      } // cell: Return shadow attribute.
        auto& stl()        { return st.token;      } // cell: Return style token.
        auto& stl() const  { return st.token;      } // cell: Return style token.
        auto link() const  { return id;            } // cell: Return object ID.
        auto isspc() const { return gc.is_space(); } // cell: Return true if char is whitespace.
        auto isnul() const { return gc.is_null();  } // cell: Return true if char is null.
        auto issame_visual(cell const& c) const // cell: Is the cell visually identical.
        {
            if (gc == c.gc || (isspc() && c.isspc()))
            {
                if (uv.bg == c.uv.bg)
                {
                    if (xy() == 0 || txt().front() == ' ')
                    {
                        return true;
                    }
                    else
                    {
                        return uv.fg == c.uv.fg;
                    }
                }
            }
            return faux;
        }
        auto set_cursor(si32 style, cell color = {})
        {
            st.cur(style);
            st.cursor_color(color.uv.bg, color.uv.fg);
        }
        auto cursor_color() const
        {
            //todo support for multiple cursor inside the cell
            return st.cursor_color();
        }
        // cell: Return whitespace cell.
        cell spc() const
        {
            return cell{ *this }.txt(whitespace);
        }
        // cell: Return empty cell.
        cell nul() const
        {
            return cell{ *this }.txt('\0');
        }
        // cell: Return dry empty cell.
        cell dry() const
        {
            return cell{ '\0' }.clr(*this);
        }
        friend auto& operator << (std::ostream& s, cell const& c)
        {
            return s << "\n\tfgc " << c.fgc()
                     << "\n\tbgc " << c.bgc()
                     << "\n\ttxt " <<(c.isspc() ? text{ "whitespace" } : utf::debase<faux, faux>(c.txt()))
                     << "\n\tmtx " <<(c.mtx())
                     << "\n\tstk " <<(c.stk() ? "true" : "faux")
                     << "\n\titc " <<(c.itc() ? "true" : "faux")
                     << "\n\tovr " <<(c.ovr() ? "true" : "faux")
                     << "\n\tblk " <<(c.blk() ? "true" : "faux")
                     << "\n\tinv " <<(c.inv() ? "true" : "faux")
                     << "\n\tbld " <<(c.bld() ? "true" : "faux")
                     << "\n\tund " <<(c.und() == unln::none   ? "none"
                                    : c.und() == unln::line   ? "line"
                                    : c.und() == unln::biline ? "biline"
                                    : c.und() == unln::wavy   ? "wavy"
                                    : c.und() == unln::dotted ? "dotted"
                                    : c.und() == unln::dashed ? "dashed"
                                                              : "unknown");
        }

        class shaders
        {
        public:
            template<class Func>
            struct brush_t
            {
                template<class Cell>
                struct func
                {
                    Cell brush;
                    static constexpr auto f = Func{};
                    constexpr func(Cell const& c)
                        : brush{ c }
                    { }
                    template<class D>
                    inline void operator () (D& dst) const
                    {
                        f(dst, brush);
                    }
                };
            };

        private:
            struct contrast_t : public brush_t<contrast_t>
            {
                static constexpr auto threshold = argb{ tint::whitedk }.luma() - 0xF;
                template<class C>
                constexpr inline auto operator () (C brush) const
                {
                    return func<C>(brush);
                }
                static inline auto invert(argb color)
                {
                    return color.luma() >= threshold ? 0xFF000000
                                                     : 0xFFffffff;
                }
                template<class D, class S>
                inline void operator () (D& dst, S& src) const
                {
                    if (src.isnul()) return;
                    auto& fgc = src.fgc();
                    if (fgc.chan.a == 0x00)
                    {
                        auto& bgc = dst.bgc();
                        if (bgc.chan.a < 2) dst.fgc(0xFFffffff);
                        else                dst.fgc(invert(bgc));
                    }
                    dst.fusefull(src);
                }
            };
            struct lite_t : public brush_t<lite_t>
            {
                template<class C> constexpr inline auto operator () (C brush) const { return func<C>(brush); }
                template<class D, class S>  inline void operator () (D& dst, S& src) const { dst.lite(src); }
            };
            struct flat_t : public brush_t<flat_t>
            {
                template<class C> constexpr inline auto operator () (C brush) const { return func<C>(brush); }
                template<class D, class S>  inline void operator () (D& dst, S& src) const { dst.set(src); }
            };
            struct mix_t : public brush_t<mix_t>
            {
                template<class C> constexpr inline auto operator () (C brush) const { return func<C>(brush); }
                template<class D, class S>  inline void operator () (D& dst, S& src) const { dst.mix(src); }
            };
            struct blend_t : public brush_t<blend_t>
            {
                template<class C> constexpr inline auto operator () (C brush) const { return func<C>(brush); }
                template<class D, class S>  inline void operator () (D& dst, S& src) const { dst.blend(src); }
            };
            struct blendpma_t : public brush_t<blendpma_t>
            {
                template<class C> constexpr inline auto operator () (C brush) const { return func<C>(brush); }
                template<class D, class S>  inline void operator () (D& dst, S& src) const { dst.blend_pma(src); }
            };
            struct alpha_t : public brush_t<alpha_t>
            {
                template<class C> constexpr inline auto operator () (C brush) const { return func<C>(brush); }
                template<class D, class S>  inline void operator () (D& dst, S& src) const { dst.alpha_sum(src); }
            };
            struct alphamix_t : public brush_t<alphamix_t>
            {
                template<class C> constexpr inline auto operator () (C brush) const { return func<C>(brush); }
                template<class D, class S>  inline void operator () (D& dst, S& src) const { argb::alpha_mix(src, dst); }
            };
            struct full_t : public brush_t<full_t>
            {
                template<class C> constexpr inline auto operator () (C brush) const { return func<C>(brush); }
                template<class D, class S>  inline void operator () (D& dst, S& src) const { dst = src; }
            };
            struct wipe_t
            {
                template<class D>  inline void operator () (D& dst) const { dst = {}; }
            };
            struct skipnulls_t : public brush_t<skipnulls_t>
            {
                template<class C> constexpr inline auto operator () (C brush) const { return func<C>(brush); }
                template<class D, class S>  inline void operator () (D& dst, S& src) const { dst.skipnulls(src); }
            };
            struct fuse_t : public brush_t<fuse_t>
            {
                template<class C> constexpr inline auto operator () (C brush) const { return func<C>(brush); }
                template<class D, class S>  inline void operator () (D& dst, S& src) const { dst.fuse(src); }
            };
            struct fuseid_t : public brush_t<fuseid_t>
            {
                template<class C> constexpr inline auto operator () (C brush) const { return func<C>(brush); }
                template<class D, class S>  inline void operator () (D& dst, S& src) const { dst.fuseid(src); }
            };
            struct fusefull_t : public brush_t<fusefull_t>
            {
                template<class C> constexpr inline auto operator () (C brush) const { return func<C>(brush); }
                template<class D, class S>  inline void operator () (D& dst, S& src) const { dst.fusefull(src); }
            };
            struct overlay_t : public brush_t<overlay_t>
            {
                template<class C> constexpr inline auto operator () (C brush) const { return func<C>(brush); }
                template<class D, class S>  inline void operator () (D& dst, S& src) const { dst.overlay(src); }
            };
            struct text_t : public brush_t<text_t>
            {
                template<class C> constexpr inline auto operator () (C brush) const { return func<C>(brush); }
                template<class D, class S>  inline void operator () (D& dst, S& src) const { dst.txt(src); }
            };
            struct meta_t : public brush_t<meta_t>
            {
                template<class C> constexpr inline auto operator () (C brush) const { return func<C>(brush); }
                template<class D, class S>  inline void operator () (D& dst, S& src) const { dst.meta(src); }
            };
            struct xlight_t
            {
                si32 factor; // Uninitialized.
                template<class T>
                inline auto operator [] (T param) const
                {
                    return xlight_t{ param };
                }
                template<class D> inline void operator () (D& dst) const { dst.xlight(factor); }
                template<class D, class S> inline void operator () (D& dst, S& src) const { dst.fuse(src); operator()(dst); }
            };
            struct underlight_t
            {
                si32 factor; // Uninitialized.
                template<class T>
                inline auto operator [] (T param) const
                {
                    return underlight_t{ param };
                }
                template<class D> inline void operator () (D& dst) const { dst.underlight(factor); }
                template<class D, class S> inline void operator () (D& dst, S& src) const { dst.fuse(src); operator()(dst); }
            };
            struct invert_t
            {
                template<class D> inline void operator () (D& dst) const { dst.invert(); }
                template<class D, class S> inline void operator () (D& dst, S& src) const { dst.fuse(src); operator()(dst); }
            };
            struct reverse_t
            {
                template<class D> inline void operator () (D& dst) const { dst.reverse(); }
                template<class D, class S> inline void operator () (D& dst, S& src) const { dst.fuse(src); operator()(dst); }
            };
            struct invbit_t
            {
                template<class D> inline void operator () (D& dst) const { dst.invbit(); }
            };
            struct disabled_t
            {
                template<class T>
                inline auto operator [] (T /*param*/) const
                {
                    return disabled_t{};
                }
                template<class D> inline void operator () (D& dst) const { dst.disabled(); }
            };
            struct transparent_t : public brush_t<transparent_t>
            {
                si32 alpha;
                constexpr transparent_t(si32 alpha)
                    : alpha{ alpha }
                { }
                template<class C> constexpr inline auto operator () (C brush) const { return func<C>(brush); }
                template<class D, class S>  inline void operator () (D& dst, S& src) const { dst.mixfull(src, alpha); }
            };
            struct xlucent_t
            {
                si32 alpha;
                constexpr xlucent_t(si32 alpha)
                    : alpha{ alpha }
                { }
                template<class D, class S>  inline void operator () (D& dst, S& src) const { dst.fuse(src); dst.bga(alpha); }
                template<class D>           inline void operator () (D& dst)         const { dst.bga(alpha); }
            };
            struct shadow_t
            {
                si32 shadow_index;
                constexpr shadow_t(si32 shadow_index)
                    : shadow_index{ std::clamp(shadow_index, 0, 255) }
                { }
                template<class D, class S>  inline void operator () (D& dst, S& src) const { dst.fuse(src); dst.st.fuse_dim(shadow_index); }
                template<class D>           inline void operator () (D& dst)         const { dst.st.fuse_dim(shadow_index); }
            };
            struct color_t
            {
                clrs colors;
                si32 factor;
                template<class T>
                constexpr color_t(T colors, si32 factor = 1)
                    : colors{ colors },
                      factor{ factor }
                { }
                constexpr color_t(cell const& brush, si32 factor = 1)
                    : colors{ brush.uv },
                      factor{ factor }
                { }
                template<class T>
                inline auto operator [] (T param) const
                {
                    return color_t{ colors, param };
                }
                template<class D>
                inline void operator () (D& dst) const
                {
                    auto b = dst.inv() ? dst.fgc() : dst.bgc();
                    dst.uv = colors;
                    //if (b == colors.bg) dst.xlight();
                    if (b == colors.bg) dst.uv.bg.shadow();
                }
                template<class D, class S>
                inline void operator () (D& dst, S& src) const
                {
                    auto i = factor;
                    while(i-- > 0) dst.fuse(src);
                    operator()(dst);
                }
            };
            struct mimic_t
            {
                clrs color;
                body style;
                constexpr mimic_t(cell const& brush)
                    : color{ brush.uv },
                      style{ brush.st }
                { }
                template<class D>
                inline void operator () (D& dst) const
                {
                    dst.uv = color;
                    dst.st.meta(style);
                }
                template<class D, class S>
                inline void operator () (D& dst, S& src) const
                {
                    operator()(dst);
                    dst.fuse(src);
                }
            };
            struct onlyid_t
            {
                id_t id;
                constexpr onlyid_t(id_t id)
                    : id{ id }
                { }
                template<class D>
                inline void operator () (D& dst) const
                {
                    dst.link(id);
                }
                template<class D, class S>
                inline void operator () (D& dst, S& src) const
                {
                    dst.fuse(src, id);
                }
            };

        public:
            static constexpr auto       color(auto brush) { return       color_t{ brush }; }
            static constexpr auto       mimic(auto brush) { return       mimic_t{ brush }; }
            static constexpr auto transparent(si32     a) { return transparent_t{ a     }; }
            static constexpr auto     xlucent(si32     a) { return     xlucent_t{ a     }; }
            static constexpr auto      onlyid(id_t newid) { return      onlyid_t{ newid }; }
            static constexpr auto      shadow(si32 index) { return      shadow_t{ index }; }
            static constexpr auto   contrast =   contrast_t{};
            static constexpr auto   fusefull =   fusefull_t{};
            static constexpr auto    overlay =    overlay_t{};
            static constexpr auto     fuseid =     fuseid_t{};
            static constexpr auto        mix =        mix_t{};
            static constexpr auto   blendpma =   blendpma_t{};
            static constexpr auto      blend =      blend_t{};
            static constexpr auto      alpha =      alpha_t{};
            static constexpr auto   alphamix =   alphamix_t{};
            static constexpr auto       lite =       lite_t{};
            static constexpr auto       fuse =       fuse_t{};
            static constexpr auto       flat =       flat_t{};
            static constexpr auto       full =       full_t{};
            static constexpr auto       wipe =       wipe_t{};
            static constexpr auto  skipnulls =  skipnulls_t{};
            static constexpr auto       text =       text_t{};
            static constexpr auto       meta =       meta_t{};
            static constexpr auto     invert =     invert_t{};
            static constexpr auto    reverse =    reverse_t{};
            static constexpr auto     invbit =     invbit_t{};
            static constexpr auto   disabled =   disabled_t{};
            static constexpr auto     xlight =     xlight_t{ 1 };
            static constexpr auto underlight = underlight_t{ 1 };
        };

        auto draw_cursor()
        {
            auto [cursor_bgc, cursor_fgc] = cursor_color();
            switch (st.cur())
            {
                case text_cursor::block:
                    if (cursor_bgc.chan.a == 0)
                    {
                        auto b = inv() ? fgc() : bgc();
                        auto f = cursor_fgc.chan.a ? cursor_fgc : b;
                        inv(faux).fgc(f).bgc(cell::shaders::contrast.invert(b));
                    }
                    else
                    {
                        auto b = cursor_bgc;
                        auto f = cursor_fgc.chan.a ? cursor_fgc : cell::shaders::contrast.invert(b);
                        inv(faux).fgc(f).bgc(b);
                    }
                    break;
                case text_cursor::I_bar:
                case text_cursor::underline:
                    if (cursor_bgc.chan.a == 0)
                    {
                        if (und() == unln::line)
                        {
                            und(unln::none);
                        }
                        else
                        {
                            auto b = inv() ? fgc() : bgc();
                            auto u = argb{ cell::shaders::contrast.invert(b) };
                            und(unln::line).unc(u);
                        }
                    }
                    else
                    {
                        auto u = cursor_bgc.to_256cube();
                        if (u == unc() && und() == unln::line) und(unln::none);
                        else                                   und(unln::line).unc(u);
                    }
                    break;
            }
        }
    };

    enum class bias : byte { none, left, right, center, };
    enum class wrap : byte { none, on,  off,            };
    enum class rtol : byte { none, rtl, ltr,            };

    namespace mime
    {
        static constexpr auto _counter = __COUNTER__ + 1;
        static constexpr auto disabled = __COUNTER__ - _counter;
        static constexpr auto textonly = __COUNTER__ - _counter;
        static constexpr auto ansitext = __COUNTER__ - _counter;
        static constexpr auto richtext = __COUNTER__ - _counter;
        static constexpr auto htmltext = __COUNTER__ - _counter;
        static constexpr auto safetext = __COUNTER__ - _counter; // mime: Sensitive textonly data.
        static constexpr auto count    = __COUNTER__ - _counter;

        namespace tag
        {
            static constexpr auto text = "text/plain"sv;
            static constexpr auto ansi = "text/xterm"sv;
            static constexpr auto html = "text/html"sv;
            static constexpr auto rich = "text/rtf"sv;
            static constexpr auto safe = "text/protected"sv;
        }

        auto meta(twod size, si32 form) // mime: Return clipdata's meta data.
        {
            return utf::concat(form == htmltext ? tag::html
                             : form == richtext ? tag::rich
                             : form == ansitext ? tag::ansi
                             : form == safetext ? tag::safe
                                                : tag::text, "/", size.x, "/", size.y);
        }
    }

    namespace misc //todo classify
    {
        template<class T>
        struct shadow
        {
            T  bitmap{};
            bool sync{};
            bool hide{};
            twod over{};
            twod step{};

            shadow()              = default;
            shadow(shadow&&)      = default;
            shadow(shadow const&) = default;
            shadow(fp32 bias, fp32 alfa, si32 size, twod offset, twod ratio, auto fuse)
            {
                generate(bias, alfa, size, offset, ratio, fuse);
            }
            // shadow: Generate shadow sprite.
            void generate(fp32 bias, fp32 alfa, si32 size, twod offset, twod ratio, auto fuse)
            {
                //bias    += _k0 * 0.1f;
                //opacity += _k1 * 1.f;
                //size    += _k2;
                //offset  +=  ratio * _k3;
                sync = true;
                alfa = std::clamp(alfa, 0.f, 255.f);
                size = std::abs(size);
                over = ratio * (size * 2);
                step = over / 2 - offset;
                auto spline = netxs::spline01{ bias };
                auto sz = ratio * (size * 2 + 1);
                if (sz.x <= 1 || sz.y <= 1) return;
                bitmap.size(sz);
                auto it = bitmap.begin();
                for (auto y = 0.f; y < sz.y; y++)
                {
                    auto y0 = y / (sz.y - 1.f);
                    auto sy = spline(y0);
                    for (auto x = 0.f; x < sz.x; x++)
                    {
                        auto x0 = x / (sz.x - 1.f);
                        auto sx = spline(x0);
                        auto xy = sy * sx; // argb::gamma(sy * sx);
                        auto a = (byte)std::round(alfa * xy);
                        fuse(*it++, a);
                    }
                }
            }
            // shadow: Render a rectangular shadow for the window rectangle.
            auto render(auto&& canvas, auto clip, auto window, auto fx)
            {
                auto dst = rect{ window.coor - over / 2, window.size + over };
                if (!dst.trim(clip)) return;
                auto basis = step - window.coor;
                clip.coor += basis;
                canvas.step(basis);
                dst.coor = dot_00;
                auto src = bitmap.area();
                auto cut = std::min(dot_00, (dst.size - src.size * 2 - dot_11) / 2);
                auto off = dent{ 0, cut.x, 0, cut.y };
                src += off;
                auto mid = rect{ src.size, std::max(dot_00, dst.size - src.size * 2) };
                auto top = rect{ twod{ src.size.x, 0 }, { mid.size.x, src.size.y }};
                auto lft = rect{ twod{ 0, src.size.y }, { src.size.x, mid.size.y }};
                if (auto m = mid.trim(clip))
                {
                    auto base_shadow = bitmap[src.size - dot_11];
                    netxs::onrect(canvas, m, fx(base_shadow));
                }
                if (top)
                {
                    auto pen = rect{{ src.size.x - 1, 0 }, { 1, src.size.y }};
                    netxs::xform_scale(canvas, top, clip, bitmap, pen, fx);
                    top.coor.y += mid.size.y + top.size.y;
                    netxs::xform_scale(canvas, top, clip, bitmap, pen.rotate({ 1, -1 }), fx);
                }
                if (lft)
                {
                    auto pen = rect{{ 0, src.size.y - 1 }, { src.size.x, 1 }};
                    netxs::xform_scale(canvas, lft, clip, bitmap, pen, fx);
                    lft.coor.x += mid.size.x + lft.size.x;
                    netxs::xform_scale(canvas, lft, clip, bitmap, pen.rotate({ -1, 1 }), fx);
                }
                auto dir = dot_11;
                            netxs::xform_mirror(canvas, clip, dst.rotate(dir).coor, bitmap, src.rotate(dir), fx);
                dir = -dir; netxs::xform_mirror(canvas, clip, dst.rotate(dir).coor, bitmap, src.rotate(dir), fx);
                dir.x += 2; netxs::xform_mirror(canvas, clip, dst.rotate(dir).coor, bitmap, src.rotate(dir), fx);
                dir = -dir; netxs::xform_mirror(canvas, clip, dst.rotate(dir).coor, bitmap, src.rotate(dir), fx);
                canvas.step(-basis);
            }
        };

        struct szgrips
        {
            twod origin; // szgrips: Grab's initial coord info.
            twod dtcoor; // szgrips: The form coor parameter change factor while resizing.
            twod sector; // szgrips: Active quadrant, x,y = {-1|+1}. Border widths.
            rect hzgrip; // szgrips: Horizontal grip.
            rect vtgrip; // szgrips: Vertical grip.
            twod widths; // szgrips: Grip's widths.
            bool inside; // szgrips: Is active.
            bool seized; // szgrips: Is seized.
            rect zoomsz; // szgrips: Captured area for zooming.
            dent zoomdt; // szgrips: Zoom step.
            bool zoomon; // szgrips: Zoom in progress.
            twod zoomat; // szgrips: Zoom pivot.

            szgrips()
                : inside{ faux },
                  seized{ faux },
                  zoomon{ faux }
            { }

            operator bool () { return inside || seized; }
            auto corner(twod length) const
            {
                return dtcoor.less(dot_11, length, dot_00);
            }
            auto quantize(twod curpos, twod basis, twod cell_size) const
            {
                curpos -= basis;
                curpos -= (curpos + cell_size/*to avoid negative values*/) % cell_size;
                return curpos;
            }
            auto grab(rect window, twod curpos, dent outer, twod cell_size = dot_11)
            {
                if (inside)
                {
                    auto outer_rect = window + outer;
                    curpos = quantize(curpos, outer_rect.coor, cell_size);
                    origin = curpos - corner(outer_rect.size);
                    seized = true;
                }
                return seized;
            }
            auto leave()
            {
                auto inside_old = std::exchange(inside, faux);
                auto changed = inside_old != inside;
                return changed;
            }
            auto calc(rect window, twod curpos, dent outer, dent inner, twod cell_size = dot_11)
            {
                auto border = outer - inner;
                auto inside_old = inside;
                auto hzgrip_old = hzgrip;
                auto vtgrip_old = vtgrip;
                auto inner_rect = window + inner;
                auto outer_rect = window + outer;
                inside = !inner_rect.hittest(curpos) && outer_rect.hittest(curpos);
                auto& length = outer_rect.size;
                curpos = quantize(curpos, outer_rect.coor, cell_size);
                auto center = std::max(length / 2, dot_11);
                if (!seized)
                {
                    dtcoor = curpos.less(center + (length & 1), dot_11, dot_00);
                    sector = dtcoor.less(dot_11, -dot_11, dot_11);
                    widths = sector.less(dot_00, twod{-border.r,-border.b },
                                                 twod{ border.l, border.t });
                }
                auto l = sector * (curpos - corner(length));
                auto a = center * l / center;
                auto b = center *~l /~center;
                auto s = sector * std::max(dot_00, a - b + center + sector.less(dot_00, dot_00, cell_size));

                hzgrip.coor.x = widths.x;     // |----|
                hzgrip.coor.y = 0;            // |    |
                hzgrip.size.y = widths.y;     // |----|
                hzgrip.size.x = s.x - s.x % cell_size.x;

                vtgrip.coor = dot_00;
                vtgrip.size = widths;
                vtgrip.size.y += s.y - s.y % cell_size.y;
                auto changed = inside_old != inside || (inside && (hzgrip_old != hzgrip || vtgrip_old != vtgrip));
                return changed;
            }
            auto drag(rect window, twod curpos, dent outer, bool zoom, twod cell_size = dot_11) const
            {
                auto outer_rect = window + outer;
                curpos = quantize(curpos, outer_rect.coor, cell_size);
                auto delta = (corner(outer_rect.size) + origin - curpos) * sector;
                if (zoom) delta *= 2;
                auto preview_step = zoom ? -delta / 2 : -delta * dtcoor;
                auto preview_area = rect{ window.coor + preview_step, window.size + delta };
                return std::pair{ preview_area, delta };
            }
            auto move(twod dxdy, bool zoom) const
            {
                auto step = zoom ? -dxdy / 2 : -dxdy * dtcoor;
                return step;
            }
            void drop()
            {
                seized = faux;
            }
            auto layout(rect area) const
            {
                auto vertex = corner(area.size);
                auto side_x = hzgrip.shift(vertex).normalize_itself().shift_itself(area.coor).trim(area);
                auto side_y = vtgrip.shift(vertex).normalize_itself().shift_itself(area.coor).trim(area);
                return std::pair{ side_x, side_y };
            }
            auto draw(auto& canvas, rect area, auto fx) const
            {
                auto [side_x, side_y] = layout(area);
                netxs::onrect(canvas, side_x, fx);
                netxs::onrect(canvas, side_y, fx);
            }
        };

        void fill(auto&& canvas, auto block, auto fx) // gfx: Fill block.
        {
            block.normalize_itself();
            netxs::onrect(canvas, block, fx);
        }
        void fill(auto&& canvas, auto fx) // gfx: Fill canvas area.
        {
            netxs::onrect(canvas, canvas.area(), fx);
        }
        void cage(auto&& canvas, rect area, dent border, auto fx) // core: Draw cage inside the specified area.
        {
            auto temp = area;
            temp.size.y = std::max(0, border.t); // Top
            fill(canvas, temp.trim(area), fx);
            temp.coor.y += area.size.y - border.b; // Bottom
            temp.size.y = std::max(0, border.b);
            fill(canvas, temp.trim(area), fx);
            temp.size.x = std::max(0, border.l); // Left
            temp.size.y = std::max(0, area.size.y - border.t - border.b);
            temp.coor.y = area.coor.y + border.t;
            fill(canvas, temp.trim(area), fx);
            temp.coor.x += area.size.x - border.r; // Right
            temp.size.x = std::max(0, border.r);
            fill(canvas, temp.trim(area), fx);
        }
    }

    using vrgb = netxs::raw_vector<irgb<si32>>;

    // canvas: Core grid.
    class core
    {
        core(twod coor, twod size, cell const& brush) // Prefill canvas using brush.
            : region{ coor, size },
              client{ dot_00, size },
              canvas(size.x * size.y, brush),
              marker{ brush }
        { }
        core(twod coor, twod size) // Prefill canvas using zero.
            : region{ coor, size },
              client{ dot_00, size },
              canvas(size.x * size.y)
        { }

    public:
        using span = std::span<cell const>;
        using body = std::vector<cell>;

    protected:
        si32 digest = 0; // core: Resize stamp.
        rect region; // core: Physical square of canvas relative to current basis (top-left corner of the current rendering object, see face::change_basis).
        rect client; // core: Active canvas area relative to current basis.
        body canvas; // core: Cell data.
        cell marker; // core: Current brush.

    public:
        core()                         = default;
        core(core&&)                   = default;
        core(core const&)              = default;
        core& operator = (core&&)      = default;
        core& operator = (core const&) = default;
        core(span cells, twod size)
            : region{ dot_00, size },
              client{ dot_00, size },
              canvas( cells.begin(), cells.end() )
        {
            assert(size.x * size.y == std::distance(cells.begin(), cells.end()));
        }
        core(cell const& fill, si32 length)
            : region{ dot_00, { length, 1 } },
              client{ dot_00, { length, 1 } },
              canvas( length, fill )
        { }
        core(cell const& fill)
            : region{ dot_00, dot_01 },
              client{ dot_00, dot_01 },
              marker{ fill }
        { }

        template<class P>
        auto same(core const& c, P compare) const // core: Compare content.
        {
            if (region.size != c.region.size) return faux;
            auto dest = c.canvas.begin();
            auto head =   canvas.begin();
            auto tail =   canvas.end();
            while (head != tail)
            {
                if (!compare(*head++, *dest++)) return faux;
            }
            return true;
        }
        auto volume() const // core: Return cell count.
        {
            return canvas.size();
        }
        auto operator == (core const& c) const { return same(c, [](auto const& a, auto const& b){ return a == b;        }); }
        auto  same       (core const& c) const { return same(c, [](auto const& a, auto const& b){ return a.same_txt(b); }); }
        constexpr auto& size() const           { return region.size;                                                        }
        auto& coor() const                     { return region.coor;                                                        }
        auto& area() const                     { return region;                                                             }
        auto  area(rect new_area)              { size(new_area.size); move(new_area.coor); clip(new_area);                  }
        auto  area(rect new_area, cell c)      { size(new_area.size, c); move(new_area.coor); clip(new_area);               }
        auto& pick()                           { return canvas;                                                             }
        auto  begin()                          { return canvas.begin();                                                     }
        auto  end()                            { return canvas.end();                                                       }
        auto  begin() const                    { return canvas.begin();                                                     }
        auto  end() const                      { return canvas.end();                                                       }
        auto  begin(twod coord)                { return canvas.begin() + coord.x + coord.y * region.size.x;                 }
        auto  begin(twod coord) const          { return canvas.begin() + coord.x + coord.y * region.size.x;                 }
        auto  begin(size_t offset)             { return canvas.begin() + offset;                                            }
        auto& operator [] (twod coord)         { return*(begin(coord));                                                     }
        auto& mark()                           { return marker;                                                             } // core: Return a reference to the default cell value.
        auto& mark() const                     { return marker;                                                             } // core: Return a reference to the default cell value.
        auto& mark(cell const& new_marker)     { marker = new_marker; return marker;                                        } // core: Set the default cell value.
        void  move(twod new_coor)              { region.coor = new_coor;                                                    } // core: Change the location of the face.
        void  step(twod delta)                 { region.coor += delta;                                                      } // core: Shift location of the face by delta.
        auto& back()                           { return canvas.back();                                                      } // core: Return last cell.
        auto  link()                           { return marker.link();                                                      } // core: Return default object ID.
        void  link(id_t id)                    { marker.link(id);                                                           } // core: Set the default object ID.
        auto  link(twod coord) const           { return region.size.inside(coord) ? (*(begin(coord))).link() : 0;           } // core: Return ID of the object in cell at the specified coordinates.
        auto  clip() const                     { return client;                                                             }
        void  clip(rect new_client)            { client = new_client;                                                       }
        auto  hash() const                     { return digest;                                                             } // core: Return the digest value that associatated with the current canvas size.
        auto  hash(si32 d)                     { return digest != d ? ((void)(digest = d), true) : faux;                    } // core: Check and the digest value that associatated with the current canvas size.
        void size(twod new_size, cell const& c) // core: Resize canvas.
        {
            if (region.size(std::max(dot_00, new_size)))
            {
                client.size = region.size;
                digest++;
                canvas.assign(region.size.x * region.size.y, c);
            }
        }
        void size(twod new_size) // core: Resize canvas.
        {
            size(new_size, marker);
        }
        void size(si32 new_size_x, cell const& c) // core: Resize canvas.
        {
            region.size.x = new_size_x;
            region.size.y = 1;
            client.size = region.size;
            canvas.assign(new_size_x, c);
            digest++;
        }
        void crop(si32 new_size_x, cell const& c = {}) // core: Resize preserving textline.
        {
            region.size.x = new_size_x;
            region.size.y = 1;
            client.size = region.size;
            canvas.resize(new_size_x, c);
            digest++;
        }
        auto crop(si32 at, si32 length) const // core: Return 1D fragment.
        {
            auto fragment = core{ span{ canvas.begin() + at, canvas.begin() + at + length }, twod{ length, 1 } };
            fragment.marker = marker;
            return fragment;
        }
        void push(cell const& c) // core: Push cell back.
        {
            crop(region.size.x + 1, c);
        }
        template<bool BottomAnchored = faux>
        void crop(twod new_size, cell const& c) // core: Resize preserving bitmap.
        {
            auto block = core{ region.coor, new_size, c };
            if constexpr (BottomAnchored) block.step({ 0, region.size.y - new_size.y });
            netxs::onbody(block, *this, cell::shaders::full);
            client.size = new_size;
            swap(block);
            digest++;
        }
        template<bool BottomAnchored = faux>
        void crop(twod new_size) // core: Resize preserving bitmap.
        {
            crop<BottomAnchored>(new_size, marker);
        }
        void kill() // core: Collapse canvas to zero size (see para).
        {
            region.size.x = 0;
            client.size.x = 0;
            canvas.resize(0);
            digest++;
        }
        void wipe(cell const& c) { std::fill(canvas.begin(), canvas.end(), c); } // core: Fill canvas with specified marker.
        void wipe()              { wipe(marker); } // core: Fill canvas with default color.
        void wipe(id_t id)                         // core: Fill canvas with specified id.
        {
            auto my_id = marker.link();
            marker.link(id);
            wipe(marker);
            marker.link(my_id);
        }
        template<class P, bool Plain = std::is_same_v<void, std::invoke_result_t<P, cell&>>>
        auto each(P proc) // core: Exec a proc for each cell.
        {
            for (auto& c : canvas)
            {
                if constexpr (Plain) proc(c);
                else             if (proc(c)) return faux;
            }
            if constexpr (!Plain) return true;
        }
        void each(rect region, auto fx) // core: Exec a proc for each cell of the specified region.
        {
            netxs::onrect(*this, region, fx);
        }
        void utf8(netxs::text& crop) // core: Convert to raw utf-8 text. Ignore right halves.
        {
            each([&](cell& c){ c.scan(crop); });
        }
        auto utf8() // core: Convert to raw utf-8 text. Ignore right halves.
        {
            auto crop = netxs::text{};
            crop.reserve(canvas.size());
            each([&](cell& c){ c.scan(crop); });
            return crop;
        }
        auto copy(body& target) const // core: Copy only body of the canvas to the specified body bitmap.
        {
            target = canvas;
            return region.size;
        }
        template<class Face>
        void copy(Face& dest) const // core: Copy only body of the canvas to the specified core.
        {
            dest.size(region.size);
            dest.canvas = canvas;
        }
        void copy(core& target, auto fx) const // core: Copy the canvas to the specified target bitmap. The target bitmap must be the same size.
        {
            netxs::oncopy(target, *this, fx);
            //todo should we copy all members?
            //target.marker = marker;
            //flow::cursor
        }
        void fill(core const& block, auto fx) // core: Fill canvas by the specified block using its coordinates.
        {
            netxs::onbody(*this, block, fx);
        }
        void zoom(core const& block, auto fx) // core: Fill canvas by the stretched block.
        {
            netxs::zoomin(*this, block, fx);
        }
        void plot(core const& block, auto fx) // core: Fill the client area by the specified block with coordinates inside the canvas area.
        {
            //todo use block.client instead of block.region
            auto joint = rect{ client.coor - region.coor, client.size };
            if (joint.trimby(block.region))
            {
                auto place = joint.coor - block.region.coor;
                netxs::inbody<faux>(*this, block, joint, place, fx);
            }
        }
        auto& peek(twod p) // core: Take the cell at the specified coor.
        {
            p -= region.coor;
            auto& c = *(canvas.begin() + p.x + p.y * region.size.x);
            return c;
        }
        void fill(rect block, auto fx) // core: Process the specified region by the specified proc.
        {
            block.normalize_itself();
            netxs::onrect(*this, block, fx);
        }
        void fill(auto fx) // core: Fill the client area using lambda.
        {
            fill(clip(), fx);
        }
        void fill(cell const& c) // core: Fill the client area using brush.
        {
            fill(clip(), cell::shaders::full(c));
        }
        void grad(argb c1, argb c2) // core: Fill the specified region with the linear gradient.
        {
            auto mx = (fp32)region.size.x;
            auto my = (fp32)region.size.y;
            auto len = std::max(1.f, std::sqrt(mx * mx + my * my * 4));

            auto dr = (c2.chan.r - c1.chan.r) / len;
            auto dg = (c2.chan.g - c1.chan.g) / len;
            auto db = (c2.chan.b - c1.chan.b) / len;
            auto da = (c2.chan.a - c1.chan.a) / len;

            auto x = si32{ 0 };
            auto y = si32{ 0 };
            auto z = si32{ 0 };
            auto allfx = [&](cell& c)
            {
                auto dt = std::sqrt(x * x + z);
                auto& chan = c.bgc().chan;
                chan.r = (byte)((fp32)c1.chan.r + dr * dt);
                chan.g = (byte)((fp32)c1.chan.g + dg * dt);
                chan.b = (byte)((fp32)c1.chan.b + db * dt);
                chan.a = (byte)((fp32)c1.chan.a + da * dt);
                ++x;
            };
            auto eolfx = [&]
            {
                x = 0;
                ++y;
                z = y * y * 4;
            };
            netxs::onrect(*this, client, allfx, eolfx);
        }
        void swap(core& other) // core: Unconditionally swap canvases.
        {
            canvas.swap(other.canvas);
            std::swap(region, other.region);
        }
        auto swap(body& target) // core: Move the canvas to the specified array and return the current layout size.
        {
            if (auto size = canvas.size())
            {
                if (target.size() == size) canvas.swap(target);
                else                       target = canvas;
            }
            return region.size;
        }
        template<feed Direction>
        auto seek(si32& x, auto proc) // core: Find proc(c) == true.
        {
            if (!region) return faux;
            static constexpr auto rev = Direction == feed::fwd ? faux : true;
            x += rev ? 1 : 0;
            auto count = 0;
            auto found = faux;
            auto width = (rev ? 0 : region.size.x) - x;
            auto field = rect{ twod{ x, 0 } + region.coor, { width, 1 }}.normalize();
            auto allfx = [&](auto& c)
            {
                if (proc(c))
                {
                    found = true;
                    return true;
                }
                count++;
                return faux;
            };
            netxs::onrect<rev>(*this, field, allfx);
            if (count) count--;
            x -= rev ? count + 1 : -count;
            return found;
        }
        template<feed Direction>
        auto word(twod coord) // core: Detect a word bound.
        {
            if (!region) return 0;
            static constexpr auto rev = Direction == feed::fwd ? faux : true;
            auto stop_by_zwsp = 0;
            auto is_empty = [&](auto txt)
            {
                auto test = txt.empty()
                         || txt.front() == whitespace
                         ||(txt.front() == '^' && txt.size() == 2); // C0 characters.
                if (test) stop_by_zwsp = 5; // Don't break by zwsp.
                return test;
            };
            auto empty = [&](auto txt)
            {
                return is_empty(txt);
            };
            auto alpha = [&](auto txt)
            {
                //todo revise (https://unicode.org/reports/tr29/#Word_Boundaries)
                auto c = utf::cluster<true>(txt).attr.cdpoint;
                return (c >= '0' && c <= '9')//30-39: '0'-'9'
                     ||(c >= '@' && c <= 'Z')//40-5A: '@','A'-'Z'
                     ||(c >= 'a' && c <= 'z')//5F,61-7A: '_','a'-'z'
                     || c == '_'             //60:    '`'
                     || c == 0xA0            //A0  NO-BREAK SPACE (NBSP)
                     ||(c >= 0xC0                // C0-10FFFF: "À" - ...
                     && c < 0x2000)||(c > 0x206F // General Punctuation
                     && c < 0x2200)||(c > 0x23FF // Mathematical Operators
                     && c < 0x2500)||(c > 0x25FF // Box Drawing
                     && c < 0x2E00)||(c > 0x2E7F // Supplemental Punctuation
                     && c < 0x3000)||(c > 0x303F // CJK Symbols and Punctuation
                     && c != 0x30FB              // U+30FB ( ・ ) KATAKANA MIDDLE DOT
                     && c < 0xFE50)||(c > 0xFE6F // FE50  FE6F Small Form Variants
                     && c < 0xFF00)||(c > 0xFF0F // Halfwidth and Fullwidth Forms
                     && c < 0xFF1A)||(c > 0xFF1F //
                     && c < 0xFF3B)||(c > 0xFF40 //
                     && c < 0xFF5B)|| c > 0xFF65 //
            ;};
            auto is_email = [&](auto txt)
            {
                return !txt.empty() && txt.front() == '@';
            };
            auto email = [&](auto txt)
            {
                return !txt.empty() && (alpha(txt) || txt.front() == '.');
            };
            auto is_digit = [&](auto txt)
            {
                auto c = utf::cluster(txt).attr.cdpoint;
                return (c >= '0'    && c <= '9')
                     ||(c >= 0xFF10 && c <= 0xFF19) // U+FF10 (０) FULLWIDTH DIGIT ZERO - U+FF19 (９) FULLWIDTH DIGIT NINE
                     || c == '.';
            };
            auto digit = [&](auto txt)
            {
                auto c = utf::cluster(txt).attr.cdpoint;
                return c == '.'
                    ||(c >= 'a' && c <= 'f')
                    ||(c >= 'A' && c <= 'F')
                    ||(c >= '0' && c <= '9')
                    ||(c >= 0xFF10 && c <= 0xFF19); // U+FF10 (０) FULLWIDTH DIGIT ZERO - U+FF19 (９) FULLWIDTH DIGIT NINE
            };
            auto func = [&](auto check)
            {
                static constexpr auto right_half = rev ? 1 : 2;
                coord.x += rev ? 1 : 0;
                auto count = decltype(coord.x){};
                auto width = (rev ? 0 : region.size.x) - coord.x;
                auto field = rect{ coord + region.coor, { width, 1 }}.normalize();
                auto allfx = [&](auto& c)
                {
                    auto txt = c.txt();
                    auto has_zwsp = stop_by_zwsp <= 0 && txt.ends_with("\u200b");
                    if (has_zwsp || (stop_by_zwsp && stop_by_zwsp < 2))
                    {
                        if constexpr (rev) stop_by_zwsp += 2; // Break here.
                        else               stop_by_zwsp++;    // Include current cluster.
                    }
                    auto [w, h, x, y] = c.whxy();
                    auto not_right_half = w != 2 || x != right_half;
                    if (stop_by_zwsp == 2 || (not_right_half && !check(txt))) return true;
                    count++;
                    return faux;
                };
                netxs::onrect<rev>(*this, field, allfx);
                if (count) count--;
                coord.x -= rev ? count + 1 : -count;
            };

            coord = std::clamp(coord, dot_00, region.size - dot_11);
            auto test = begin(coord)->txt();
            if constexpr (rev)
            {
                if (test.ends_with("\u200b")) stop_by_zwsp -= 2; // Skip zwsp in the first cell.
            }
            is_digit(test) ? func(digit) :
            is_email(test) ? func(email) :
            is_empty(test) ? func(empty) :
                             func(alpha);
            return coord.x;
        }
        template<feed Direction>
        auto word(si32 offset) // core: Detect a word bound.
        {
            return word<Direction>(twod{ offset, 0 });
        }
        void cage(rect area, dent border, auto fx) // core: Draw the cage around specified area.
        {
            netxs::misc::cage(*this, area, border, fx);
        }
        void cage(rect area, twod border_width, auto fx) // core: Draw the cage around specified area.
        {
            cage(area, dent{ border_width.x, border_width.x, border_width.y, border_width.y }, fx);
        }
        template<class Text, class P = noop>
        void text(twod pos, Text const& txt, bool rtl = faux, P print = {}) // core: Put the specified text substring to the specified coordinates on the canvas.
        {
            rtl ? txt.template output<true>(*this, pos, print)
                : txt.template output<faux>(*this, pos, print);
        }
        template<class Si32>
        auto find(core const& what, Si32&& from, feed dir = feed::fwd) const // core: Find the substring and place its offset in &from.
        {
            assert(     canvas.size() <= si32max);
            assert(what.canvas.size() <= si32max);
            auto full = (si32)     canvas.size();
            auto size = (si32)what.canvas.size();
            auto rest = full - from;
            auto look = [&](auto canvas_begin, auto canvas_end, auto what_begin)
            {
                if (!size || size > rest) return faux;

                size--;
                auto head = canvas_begin;
                auto tail = canvas_end - size;
                auto iter = head + from;
                auto base = what_begin;
                auto dest = base;
                auto&test =*base;
                while (iter != tail)
                {
                    if (test.same_fragment(*iter++))
                    {
                        auto init = iter;
                        auto stop = iter + size;
                        while (init != stop && init->same_fragment(*++dest))
                        {
                            ++init;
                        }

                        if (init == stop)
                        {
                            from = (si32)std::distance(head, iter) - 1;
                            return true;
                        }
                        else dest = base;
                    }
                }
                return faux;
            };

            if (dir == feed::fwd)
            {
                if (look(canvas.begin(), canvas.end(), what.canvas.begin()))
                {
                    return true;
                }
            }
            else
            {
                std::swap(rest, from); // Reverse.
                if (look(canvas.rbegin(), canvas.rend(), what.canvas.rbegin()))
                {
                    from = full - from - 1; // Restore forward representation.
                    return true;
                }
            }
            return faux;
        }
        auto toxy(si32 offset) const // core: Convert offset to coor.
        {
            assert(canvas.size() <= si32max);
            auto maxs = (si32)canvas.size();
            if (!maxs) return dot_00;
            offset = std::clamp(offset, 0, maxs - 1);
            auto sx = std::max(1, region.size.x);
            return twod{ offset % sx, offset / sx };
        }
        auto line(si32 from, si32 upto) const // core: Get stripe.
        {
            if (from > upto) std::swap(from, upto);
            assert(canvas.size() <= si32max);
            auto maxs = (si32)canvas.size();
            from = std::clamp(from, 0, maxs ? maxs - 1 : 0);
            upto = std::clamp(upto, 0, maxs);
            auto size = upto - from;
            return core{ span{ canvas.begin() + from, (size_t)size }, { size, 1 }};
        }
        auto line(twod p1, twod p2) const // core: Get stripe.
        {
            if (p1.y > p2.y || (p1.y == p2.y && p1.x > p2.x)) std::swap(p1, p2);
            auto from = p1.x + p1.y * region.size.x;
            auto upto = p2.x + p2.y * region.size.x + 1;
            return line(from, upto);
        }
        auto tile(core& image, auto fx) // core: Tile with a specified bitmap.
        {
            auto step = image.size();
            auto grid = netxs::grid_mod(region.coor, step);
            auto init = region.coor - grid - region.coor.less(dot_00, step, dot_00);
            auto coor = init;
            auto stop = region.coor + region.size;
            while (coor.y < stop.y)
            {
                while (coor.x < stop.x)
                {
                    image.move(coor);
                    fill(image, fx);
                    coor.x += step.x;
                }
                coor.x = init.x;
                coor.y += step.y;
            }
        }
        void operator += (core const& src) // core: Append specified canvas.
        {
            //todo inbody::RTL
            auto a_size = size();
            auto b_size = src.size();
            auto new_sz = twod{ a_size.x + b_size.x, std::max(a_size.y, b_size.y) };
            auto block = core{ region.coor, new_sz, marker };

            auto r = rect{{ 0, new_sz.y - a_size.y }, a_size };
            netxs::inbody<faux>(block, *this, r, dot_00, cell::shaders::full);
            r.coor.x = a_size.x;
            r.coor.y = new_sz.y - b_size.y;
            r.size = b_size;
            netxs::inbody<faux>(block, src, r, dot_00, cell::shaders::full);

            swap(block);
            digest++;
        }
    };
}

namespace netxs::misc
{
    template<si32 Repeat = 2, bool InnerGlow = faux, class T = vrgb, class P = noop, si32 Ratio = 1>
    void boxblur(auto& image, si32 r, T&& cache = {}, P shade = {})
    {
        using irgb = std::decay_t<T>::value_type;

        auto area = image.area();
        auto clip = image.clip().trim(area);
        if (!clip) return;

        auto w = std::max(0, clip.size.x);
        auto h = std::max(0, clip.size.y);
        auto s = w * h;

        if (cache.size() < (size_t)s)
        {
            cache.resize(s);
        }

        auto start = clip.coor - area.coor;
        auto s_ptr = image.begin() + start.x + area.size.x * start.y;
        auto d_ptr = cache.begin();

        auto s_width = area.size.x;
        auto d_width = clip.size.x;

        auto s_point = [](auto c)->auto& { return *c; }; //->bgc(); };
        auto d_point = [](auto c)->auto& { return *c; };

        for (auto _(Repeat); _--;) // Emulate Gaussian blur.
        netxs::boxblur<irgb, InnerGlow>(s_ptr,
                                        d_ptr, w,
                                               h, r, s_width,
                                                     d_width, Ratio, s_point,
                                                                     d_point, shade);
    }

    void contour(auto& image)
    {
        static auto shadows_cache = netxs::raw_vector<fp32>{};
        static auto boxblur_cache = netxs::raw_vector<fp32>{};
        auto r = image.area();
        auto v = r.size.x * r.size.y;
        boxblur_cache.resize(v);
        shadows_cache.resize(v);
        auto shadows_image = netxs::raster<std::span<fp32>, rect>{ shadows_cache, r };
        netxs::misc::cage(shadows_image, shadows_image.area(), dent{ 1, 0, 1, 0 }, [](auto& dst){ dst = 0.f; }); // Clear cached garbage (or uninitialized data) after previous blur (1px border at the top and left sides).
        shadows_image.step(-dot_11);
        netxs::onbody(image, shadows_image, [](auto& src, auto& dst){ dst = src ? 255.f * 3.f : 0.f; }); // Note: Pure black pixels will become invisible/transparent.
        shadows_image.step(dot_11);
        shadows_image.clip(r);
        netxs::misc::boxblur<2>(shadows_image, 1, boxblur_cache);
        netxs::oncopy(image, shadows_image, [](auto& src, auto& dst){ src.chan.a = src ? 0xFF : (byte)std::clamp(dst, 0.f, 255.f); });
    }
}