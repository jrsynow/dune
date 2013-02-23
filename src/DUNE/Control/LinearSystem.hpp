//***************************************************************************
// Copyright (C) 2007-2013 Laboratório de Sistemas e Tecnologia Subaquática *
// Departamento de Engenharia Electrotécnica e de Computadores              *
// Rua Dr. Roberto Frias, 4200-465 Porto, Portugal                          *
//***************************************************************************
// Author: Rui Gomes                                                        *
//***************************************************************************

#ifndef DUNE_CONTROL_LINEARSYSTEM_HPP_INCLUDED_
#define DUNE_CONTROL_LINEARSYSTEM_HPP_INCLUDED_

// ISO C++ 98 headers.
#include <iostream>
#include <stdexcept>
#include <cmath>

// DUNE headers.
#include <DUNE/Config.hpp>
#include <DUNE/Math/Matrix.hpp>

namespace DUNE
{
  namespace Control
  {
    // Export DLL Symbol.
    class DUNE_DLL_SYM LinearSystem;

    class LinearSystem
    {
    public:
      //! Exception raised by this class.
      class Error: public std::runtime_error
      {
      public:
        Error(const std::string& msg):
          std::runtime_error("linear system error: " + msg)
        { }
      };

      //!Default Constructor for a null system
      LinearSystem(void);

      //! Constructor for continuous system
      LinearSystem(const Math::Matrix& a, const Math::Matrix& b, const Math::Matrix& c, const Math::Matrix& d, double Ts = 0);

      //! Converts a continuous system to it's discrete representation
      LinearSystem&
      c2d(double Ts);

      //! Append a linear systems n times (increasing it's dimention)
      LinearSystem&
      append(const LinearSystem& a_sys, short n);

      //! Simulates a linear systems at the current time step considering the current state m_X0
      //! before simulating and updates both m_X0 and m_Y0 respectively with the new state and output.
      //! @param a_u the control input
      //! @param threshold input output difference threshold for conditional state update. Will not update state if difference is larger than threshold
      //! @return m_Y0 the output
      Math::Matrix
      simLinearSystem(const Math::Matrix& a_u, double threshold);

      //! Retrive A matrix
      inline const Math::Matrix&
      getA(void) const
      {
        return m_A;
      }

      //! Retrive B matrix
      inline const Math::Matrix&
      getB(void) const
      {
        return m_B;
      }

      //! Retrive C matrix
      inline const Math::Matrix&
      getC(void) const
      {
        return m_C;
      }

      //! Retrive D matrix
      inline const Math::Matrix&
      getD(void) const
      {
        return m_D;
      }

      //! Retrive currrent state vector (X0)
      inline const Math::Matrix&
      getX0(void) const
      {
        return m_X0;
      }

      //! Retrive current output vector (Y0)
      inline const Math::Matrix&
      getY0(void) const
      {
        return m_Y0;
      }

      //! Retrive number of inputs
      inline short
      getIn(void) const
      {
        return m_n_in;
      }

      //! Retrive number of outputs
      inline short
      getOut(void) const
      {
        return m_n_out;
      }

      //! Retrive number of states
      inline short
      getSt(void) const
      {
        return m_n_st;
      }

      //! Retrive discretization time step
      inline double
      getTs(void) const
      {
        return m_Ts;
      }

      //! Set currrent state vector (X0)
      inline void
      setX0(Math::Matrix& a_x0)
      {
        m_X0 = a_x0;
      }

      //! LinearSystem print
      friend DUNE_DLL_SYM std::ostream&
      operator<<(std::ostream& os, const LinearSystem& sys);

    private:
      Math::Matrix m_A; //!< Linear system matrices
      Math::Matrix m_B; //!< Linear system matrices
      Math::Matrix m_C; //!< Linear system matrices
      Math::Matrix m_D; //!< Linear system matrices
      Math::Matrix m_X0; //!< Current State
      Math::Matrix m_Y0; //!< Current Output

      short m_n_in; //!< number of inputs, outputs and states
      short m_n_out;      //!< number of inputs, outputs and states
      short m_n_st; //!< number of inputs, outputs and states

      double m_Ts;           //!< sampling time
    };
  }
}

#endif