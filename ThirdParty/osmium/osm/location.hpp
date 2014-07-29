#ifndef OSMIUM_OSM_LOCATION_HPP
#define OSMIUM_OSM_LOCATION_HPP

/*

This file is part of Osmium (http://osmcode.org/libosmium).

Copyright 2013,2014 Jochen Topf <jochen@topf.org> and others (see README).

Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

*/

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iosfwd>
#include <limits>
#include <stdexcept>
#include <string>

#include <osmium/util/operators.hpp>

namespace osmium {

    /**
     * Exception signaling an invalid location, ie a location
     * outside the -180 to 180 and -90 to 90 degree range.
     */
    struct invalid_location : public std::range_error {

        invalid_location(const std::string& what) :
            std::range_error(what) {
        }

        invalid_location(const char* what) :
            std::range_error(what) {
        }

    }; // struct invalid_location

    /**
     * Locations define a place on earth.
     *
     * Locations are stored in 32 bit integers for the x and y
     * coordinates, respectively. This gives you an accuracy of a few
     * centimeters, good enough for OSM use. (The main OSM database
     * uses the same scheme.)
     *
     * An undefined Location can be created by calling the constructor
     * without parameters.
     *
     * Coordinates are never checked on whether they are inside bounds.
     * Call valid() to check this.
     */
    class Location : osmium::totally_ordered<Location> {

        int32_t m_x;
        int32_t m_y;

    public:

        /// this value is used for a coordinate to mark it as undefined
        static constexpr int32_t undefined_coordinate = std::numeric_limits<int32_t>::max();

        static constexpr int coordinate_precision = 10000000;

        static int32_t double_to_fix(const double c) noexcept {
            return std::round(c * coordinate_precision);
        }

        static constexpr double fix_to_double(const int32_t c) noexcept {
            return static_cast<double>(c) / coordinate_precision;
        }

        /**
         * Create undefined Location.
         */
        explicit constexpr Location() :
            m_x(undefined_coordinate),
            m_y(undefined_coordinate) {
        }

        /**
         * Create Location with given x and y coordinates.
         * Note that these coordinates are coordinate_precision
         * times larger than the real coordinates.
         */
        constexpr Location(const int32_t x, const int32_t y) :
            m_x(x),
            m_y(y) {
        }

        /**
         * Create Location with given x and y coordinates.
         * Note that these coordinates are coordinate_precision
         * times larger than the real coordinates.
         */
        constexpr Location(const int64_t x, const int64_t y) :
            m_x(x),
            m_y(y) {
        }

        /**
         * Create Location with given longitude and latitude.
         */
        Location(const double lon, const double lat) :
            m_x(double_to_fix(lon)),
            m_y(double_to_fix(lat)) {
        }

        Location(const Location&) = default;
        Location(Location&&) = default;
        Location& operator=(const Location&) = default;
        Location& operator=(Location&&) = default;
        ~Location() = default;

        /**
         * Check whether the coordinates of this location
         * are defined.
         */
        explicit constexpr operator bool() const noexcept {
            return m_x != undefined_coordinate && m_y != undefined_coordinate;
        }

        /**
         * Check whether the coordinates are inside the
         * usual bounds (-180<=lon<=180, -90<=lat<=90).
         */
        constexpr bool valid() const noexcept {
            return m_x >= -180 * coordinate_precision
                && m_x <=  180 * coordinate_precision
                && m_y >=  -90 * coordinate_precision
                && m_y <=   90 * coordinate_precision;
        }

        constexpr int32_t x() const noexcept {
            return m_x;
        }

        constexpr int32_t y() const noexcept {
            return m_y;
        }

        Location& x(const int32_t x) noexcept {
            m_x = x;
            return *this;
        }

        Location& y(const int32_t y) noexcept {
            m_y = y;
            return *this;
        }

        /**
         * Get longitude.
         *
         * @throws invalid_location if the location is invalid
         */
        double lon() const {
            if (!valid()) {
                throw osmium::invalid_location("invalid location");
            }
            return fix_to_double(m_x);
        }

        /**
         * Get longitude without checking the validity.
         */
        double lon_without_check() const {
            return fix_to_double(m_x);
        }

        /**
         * Get latitude.
         *
         * @throws invalid_location if the location is invalid
         */
        double lat() const {
            if (!valid()) {
                throw osmium::invalid_location("invalid location");
            }
            return fix_to_double(m_y);
        }

        /**
         * Get latitude without checking the validity.
         */
        double lat_without_check() const {
            return fix_to_double(m_y);
        }

        Location& lon(double lon) noexcept {
            m_x = double_to_fix(lon);
            return *this;
        }

        Location& lat(double lat) noexcept {
            m_y = double_to_fix(lat);
            return *this;
        }

        static constexpr int coordinate_length =
            1 + /* sign */
            3 + /* before . */
            1 + /* . */
            7 + /* after . */
            1; /*  null byte */

        template <typename T>
        static T coordinate2string(T iterator, double value) {
            char buffer[coordinate_length];

            int len = snprintf(buffer, coordinate_length, "%.7f", value);
            while (buffer[len-1] == '0') --len;
            if (buffer[len-1] == '.') --len;

            return std::copy_n(buffer, len, iterator);
        }

        template <typename T>
        T as_string(T iterator, const char separator) const {
            iterator = coordinate2string(iterator, lon());
            *iterator++ = separator;
            return coordinate2string(iterator, lat());
        }

    }; // class Location

    /**
     * Locations are equal if both coordinates are equal.
     */
    inline constexpr bool operator==(const Location& lhs, const Location& rhs) noexcept {
        return lhs.x() == rhs.x() && lhs.y() == rhs.y();
    }

    /**
     * Compare two locations by comparing first the x and then
     * the y coordinate. If either of the locations is
     * undefined the result is undefined.
     */
    inline constexpr bool operator<(const Location& lhs, const Location& rhs) noexcept {
        return (lhs.x() == rhs.x() && lhs.y() < rhs.y()) || lhs.x() < rhs.x();
    }

    /**
     * Output a location to a stream.
     */
    template <typename TChar, typename TTraits>
    inline std::basic_ostream<TChar, TTraits>& operator<<(std::basic_ostream<TChar, TTraits>& out, const osmium::Location& location) {
        if (location) {
            out << '(' << location.lon() << ',' << location.lat() << ')';
        } else {
            out << "(undefined,undefined)";
        }
        return out;
    }

} // namespace osmium

#endif // OSMIUM_OSM_LOCATION_HPP
