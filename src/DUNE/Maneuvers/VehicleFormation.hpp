//***************************************************************************
// Copyright (C) 2007-2013 Laboratório de Sistemas e Tecnologia Subaquática *
// Departamento de Engenharia Electrotécnica e de Computadores              *
// Rua Dr. Roberto Frias, 4200-465 Porto, Portugal                          *
//***************************************************************************
// Author: Eduardo Marques                                                  *
//***************************************************************************

#ifndef DUNE_MANEUVERS_VEHICLEFORMATION_HPP_INCLUDED_
#define DUNE_MANEUVERS_VEHICLEFORMATION_HPP_INCLUDED_

// ISO C++ 98 headers.
#include <vector>
#include <map>

// DUNE headers.
#include <DUNE/IMC.hpp>
#include <DUNE/Maneuvers/Maneuver.hpp>

namespace DUNE
{
  namespace Maneuvers
  {
    // Export DLL Symbol.
    class DUNE_DLL_SYM VehicleFormation;

    //! Abstract base class for vehicle formation maneuver tasks.
    class VehicleFormation: public Maneuver
    {
    public:
      //! Constructor.
      //! @param name name.
      //! @param ctx context.
      VehicleFormation(const std::string& name, Tasks::Context& ctx);

      //! Destructor.
      virtual
      ~VehicleFormation(void);

      virtual void
      onUpdateParameters(void);

      //! Consumer for IMC::VehicleFormation message.
      //! @param msg vehicle formation message
      void
      consume(const IMC::VehicleFormation* msg);

      //! Method invoked on maneuver startup.
      //! By default the base class implementation does nothing.
      //! @param msg vehicle formation message
      virtual void
      onInit(const IMC::VehicleFormation* msg)
      {
        (void)msg;
      }

      //! Consumer for IMC::RemoteState message.
      //! @param msg message
      void
      consume(const IMC::RemoteState* msg);

      //! Abstract method invoked upon a remote state update.
      //! @param index formation index of remote vehicle
      //! @param rstate state of remote vehicle
      virtual void
      onUpdate(int index, const IMC::RemoteState& rstate) = 0;

      //! Consumer for IMC::EstimatedState message.
      //! @param msg message
      void
      consume(const IMC::EstimatedState* msg);

      //! Consumer for IMC::PathControlState message.
      //! @param msg path control state message
      void
      consume(const IMC::PathControlState* msg);

      //! Abstract method called upon path completion.
      //! This will not be called in approach stage (see isApproaching()).
      virtual void
      onPathCompletion(void) = 0;

      //! Abstract method invoked for trajectory control.
      //! This will be invoked periodically according to the
      //! control step period (see controlPeriod()) after
      //! the initial approach stage has concluded (see isApproaching()).
      virtual void
      step(const IMC::EstimatedState& state) = 0;

      //! Method called upon maneuver deactivation.
      void
      onManeuverDeactivation(void);

      //! Method invoked on maneuver reset.
      //! By default the base class implementation does nothing.
      virtual void
      onReset(void)
      { }

      //! Trajectory point.
      struct TPoint
      {
        double x; //!< X coordinate offset (North).
        double y; //!< Y coordinate offset (East).
        double z; //!< Z coordinate offset (Down).
        double t; //!< time coordinate offset.
      };

      //! Get a point in the trajectory.
      //! @param t_index index of point
      //! @param f_index formation index (vehicle)
      //! @return corresponding traj. point, optionally displaced by formation index offsets
      TPoint
      point(int t_index, int f_index = -1) const;

      //! Get number of points in the trajetory.
      inline int
      trajectory_points(void) const
      {
        return m_traj.size();
      }

      //! Participant data (per vehicle in the formation).
      struct Participant
      {
        int vid; //!< IMC id
        double x; //!< Along-track offset.
        double y; //!< Cross-track offset.
        double z; //!< Depth offset.
      };

      //! Get number of participants in formation.
      //! @return number of participants in formation
      inline int
      participants() const
      {
        return m_participants.size();
      }

      //! Get configuration of a vehicle in formation.
      //! @param index formation index
      //! @return participant data for specified formation index
      inline const Participant&
      participant(int index) const
      {
        return m_participants[index];
      }

      //! Get configuration of local vehicle in formation.
      //! @return participant data for local vehicle
      inline const Participant&
      self(void)
      {
        return m_participants[m_fidx];
      }

      //! Get index of local vehicle in formation.
      //! @return formation index.
      inline int
      formation_index(void) const
      {
        return m_fidx;
      }

      //! Get index of given IMC address in formation.
      //! @return formation index.
      inline int
      formation_index(int addr) const
      {
        std::map<int, int>::const_iterator itr = m_addr2idx.find(addr);

        if (itr == m_addr2idx.end())
          return 0xFFFF;

        return itr->second;
      }

      //! Get control step period.
      //! @return control step period.
      inline double
      controlPeriod(void) const
      {
        return m_cstep_period;
      }

      //! Check if maneuver is still in approach stage (i.e. moving to the initial point).
      //! @return true if maneuver is in approach state, false otherwise.
      inline bool
      isApproaching(void) const
      {
        return m_approach;
      }

      void
      desiredPath(const TPoint& start, const TPoint& end, double radius = -1);

      void
      desiredSpeed(double value, uint8_t speed_units);

      void
      toLocalCoordinates(double lat, double lon, double* x, double* y);

    private:
      std::vector<TPoint> m_traj; //!< Trajectory points.
      std::vector<Participant> m_participants; //!< Trajectory points.
      std::map<int, int> m_addr2idx;
      bool m_approach; //!< Approach stage flag.
      int m_fidx; //!< Formation index.
      double m_rlat; // Reference latitude set.
      double m_rlon; // Reference longitude set.
      double m_cstep_period; //! control step period
      double m_cstep_time; //! time of last control step
      IMC::DesiredPath m_path;
      IMC::DesiredZ m_depth;

      bool
      initParticipants(const IMC::VehicleFormation*);

      bool
      initTrajectory(const IMC::VehicleFormation*);
    };
  }
}

#endif