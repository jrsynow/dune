//***************************************************************************
// Copyright (C) 2007-2013 Laboratório de Sistemas e Tecnologia Subaquática *
// Departamento de Engenharia Electrotécnica e de Computadores              *
// Rua Dr. Roberto Frias, 4200-465 Porto, Portugal                          *
//***************************************************************************
// Author: Eduardo Marques                                                  *
//***************************************************************************

#ifndef DUNE_COORDINATES_GENERAL_HPP_INCLUDED_
#define DUNE_COORDINATES_GENERAL_HPP_INCLUDED_

// ISO C++ 98 headers.
#include <cmath>

// DUNE headers.
#include <DUNE/Math/Matrix.hpp>

namespace DUNE
{
  // Forward declarations.
  namespace IMC { class EstimatedState; }

  namespace Coordinates
  {
    //! Get polar coordinates corresponding to given XY coordinates.
    //! @param x X coordinate
    //! @param y Y coordinate
    //! @param angle pointer to output angle
    //! @param norm pointer to output norm
    inline
    void
    toPolar(double x, double y, double* angle, double* norm)
    {
      *angle = std::atan2(y, x);
      *norm = std::sqrt(x * x + y * y);
    }

    //! Get polar coordinates corresponding to given XY coordinates.
    //! @param coord XY coordinates
    //! @param angle pointer to output angle
    //! @param norm pointer to output norm
    template <typename A>
    inline void
    toPolar(const A& coord, double* angle, double* norm)
    {
      toPolar(coord.x, coord.y, angle, norm);
    }

    //! Get bearing and range of a point in relation to an origin.
    //! @param origin Origin
    //! @param point point for which offset is to be obtained
    //! @param bearing pointer to output bearing data
    //! @param range pointer to output range data
    template <typename A, typename B>
    inline void
    getBearingAndRange(const A& origin, const B& point, double* bearing, double* range)
    {
      toPolar(point.x - origin.x, point.y - origin.y, bearing, range);
    }

    //! Displace a XY coordinate according to given bearing and range.
    //! @param point point
    //! @param bearing bearing
    //! @param range range
    template <typename A>
    void
    displace(A& point, double bearing, double range)
    {
      point.x += range * std::cos(bearing);
      point.y += range * std::sin(bearing);
    }

    //! Calculate waypoint given waypoint origin, target bearing and range.
    //! @param origin Origin
    //! @param bearing Target bearing
    //! @param range Target range
    //! @param point output point
    template <typename A, typename B>
    void
    setBearingAndRange(const A& origin, double bearing, double range, B& point)
    {
      point.x = origin.x + range* std::cos(bearing);
      point.y = origin.y + range* std::sin(bearing);
    }

    //! Get range between two points.
    //! @param a Waypoint 1
    //! @param b Waypoint 2
    //! @return bearing from origin to point
    template <typename A, typename B>
    double
    getRange(const A& a, const B& b)
    {
      double dx = b.x - a.x, dy = b.y - a.y;
      return std::sqrt(dx * dx + dy * dy);
    }

    //! Get bearing of a point in relation to an origin.
    //! @param origin Origin
    //! @param point point for which offset is to be obtained
    //! @return bearing from origin to point
    template <typename A, typename B>
    double
    getBearing(const A& origin, const B& point)
    {
      return std::atan2(point.y - origin.y, point.x - origin.x);
    }

    //! Get along and cross track positions of a 2D point for a track
    //! defined by an origin and an orientation.
    //! @param origin origin
    //! @param orientation orientation
    //! @param point point for which track positions are required
    //! @param x along-track position on exit
    //! @param y optional cross-track position on exit
    template <typename A, typename B>
    void
    getTrackPosition(const A& origin, double orientation, const B& point, double* x, double* y = 0)
    {
      double b, r;

      getBearingAndRange(origin, point, &b, &r);
      b -= orientation;

      *x = r * std::cos(b);
      if (y)
        *y = r * std::sin(b);
    }

    //! Convert a three-dimensional vector from spherical coordinates
    //! (R,Az,El) to cartesian coordinates.
    //! @param r vector norm.
    //! @param az azimuth angle in radians.
    //! @param el elevation angle in radians.
    inline Math::Matrix
    sphericalToCartesian(double r, double az, double el)
    {
      double a = r * std::cos(el);
      double rv[] = {a* std::cos(az), a * std::sin(az), r * std::sin(el)};

      return Math::Matrix(rv, 3, 1);
    }

    //! Convert the position in an estimated state message to WGS84 coordinates.
    //! @param[in] estate estimated state message.
    //! @param[out] lat WGS84 latitude.
    //! @param[out] lon WGS84 longitude.
    //! @param[out] hae height above WGS84 ellipsoid.
    void
    toWGS84(const IMC::EstimatedState& estate, double& lat, double& lon, double& hae);

    //! Convert the position in an estimated state message to WGS84 coordinates.
    //! @param[in] estate estimated state message.
    //! @param[out] lat WGS84 latitude.
    //! @param[out] lon WGS84 longitude.
    void
    toWGS84(const IMC::EstimatedState& estate, double& lat, double& lon);
  }
}

#endif