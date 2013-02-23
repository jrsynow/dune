//***************************************************************************
// Copyright (C) 2007-2013 Laboratório de Sistemas e Tecnologia Subaquática *
// Departamento de Engenharia Electrotécnica e de Computadores              *
// Rua Dr. Roberto Frias, 4200-465 Porto, Portugal                          *
//***************************************************************************
// Author: Rui Gomes (original code)                                        *
// Author: Eduardo Marques (port)                                           *
//***************************************************************************
// Ported from Neptus (pt.up.fe.dceg.neptus.types.coord.CoordinateUtil)     *
//***************************************************************************

#ifndef DUNE_COORDINATES_BODY_FIXED_FRAME_HPP_INCLUDED_
#define DUNE_COORDINATES_BODY_FIXED_FRAME_HPP_INCLUDED_

// ISO C++ 98 headers.
#include <cmath>

// DUNE headers.
#include <DUNE/Config.hpp>

namespace DUNE
{
  namespace Coordinates
  {
    // Export DLL Symbol.
    class DUNE_DLL_SYM BodyFixedFrame;

    //! Class with utility methods for body fixed frame conversions.
    class BodyFixedFrame
    {
    public:
      //! Inertial to body frame conversion of linear velocities.
      //! Given the angles of a body in respect to the reference axes
      //! (psi, theta, psi)
      //! and its inertial frame linear velocities (u_in,v_in,w_in)
      //! calculate the body fixed linear velocities (u,v,w).
      //! @param phi pitch angle
      //! @param theta roll angle
      //! @param psi yaw angle
      //! @param vx linear velocity in x-axis in the inertial frame
      //! @param vy linear velocity in y-axis in the inertial frame
      //! @param vz linear velocity in z-axis in the inertial frame
      //! @param u on return, linear velocity in x-axis in the body-fixed frame
      //! @param v on return, linear velocity in y-axis in the body-fixed frame
      //! @param w on return, linear velocity in z-axis in the body-fixed frame
      template <typename Ta, typename Tb, typename Tc>
      static void
      toBodyFrame(Ta phi, Ta theta, Ta psi, Tb vx, Tb vy, Tb vz, Tc* u, Tc* v, Tc* w)
      {
        double cphi = std::cos(phi);
        double sphi = std::sin(phi);
        double ctheta = std::cos(theta);
        double stheta = std::sin(theta);
        double cpsi = std::cos(psi);
        double spsi = std::sin(psi);
        double t10 = vx * cpsi;
        double t12 = stheta * sphi;
        double t14 = vx * spsi;
        double t17 = vy * spsi;
        double t19 = vy * cpsi;
        double t23 = stheta * cphi;

        *u = cpsi * ctheta * vx + spsi * ctheta * vy - stheta * vz;
        *v = t10 * t12 - t14 * cphi + t17 * t12 + t19 * cphi + sphi * ctheta * vz;
        *w = t10 * t23 + t14 * sphi + t17 * t23 - t19 * sphi + cphi * ctheta * vz;
      }


      //! Body to inertial frame conversion of linear velocities.
      //! Given the angles of a body in respect to the reference axes
      //! (psi, theta, psi)
      //! and its body frame linear velocities (u,v,w)
      //! calculate the body fixed linear velocities (u_in,v_in,w_in)
      //! @param phi pitch angle
      //! @param theta roll angle
      //! @param psi yaw angle
      //! @param u linear velocity in x-axis in the body-fixed frame
      //! @param v linear velocity in y-axis in the body-fixed frame
      //! @param w linear velocity in z-axis in the inertial frame
      //! @param vx on return, linear velocity in x-axis in the inertial frame
      //! @param vy on return, linear velocity in y-axis in the inertial frame
      //! @param vz on return, linear velocity in z-axis in the inertial frame
      template <typename Ta, typename Tb, typename Tc>
      static void
      toInertialFrame(Ta phi, Ta theta, Ta psi, Tb u, Tb v, Tb w, Tc* vx, Tc* vy, Tc* vz)
      {
        double cphi = std::cos(phi);
        double ctheta = std::cos(theta);
        double cpsi = std::cos(psi);
        double sphi = std::sin(phi);
        double stheta = std::sin(theta);
        double spsi = std::sin(psi);
        double t3 = v * cpsi;
        double t4 = stheta * sphi;
        double t6 = v * spsi;
        double t8 = w * cpsi;
        double t9 = stheta * cphi;
        double t11 = w * spsi;

        *vx = cpsi * ctheta * u + t3 * t4 - t6 * cphi + t8 * t9 + t11 * sphi;
        *vy = spsi * ctheta * u + t6 * t4 + t3 * cphi + t11 * t9 - t8 * sphi;
        *vz = -stheta * u + ctheta * sphi * v + ctheta * cphi * w;
      }
    };
  }
}
#endif