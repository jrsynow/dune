//***************************************************************************
// Copyright 2007-2014 Universidade do Porto - Faculdade de Engenharia      *
// Laboratório de Sistemas e Tecnologia Subaquática (LSTS)                  *
//***************************************************************************
// This file is part of DUNE: Unified Navigation Environment.               *
//                                                                          *
// Commercial Licence Usage                                                 *
// Licencees holding valid commercial DUNE licences may use this file in    *
// accordance with the commercial licence agreement provided with the       *
// Software or, alternatively, in accordance with the terms contained in a  *
// written agreement between you and Universidade do Porto. For licensing   *
// terms, conditions, and further information contact lsts@fe.up.pt.        *
//                                                                          *
// European Union Public Licence - EUPL v.1.1 Usage                         *
// Alternatively, this file may be used under the terms of the EUPL,        *
// Version 1.1 only (the "Licence"), appearing in the file LICENCE.md       *
// included in the packaging of this file. You may not use this work        *
// except in compliance with the Licence. Unless required by applicable     *
// law or agreed to in writing, software distributed under the Licence is   *
// distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF     *
// ANY KIND, either express or implied. See the Licence for the specific    *
// language governing permissions and limitations at                        *
// https://www.lsts.pt/dune/licence.                                        *
//***************************************************************************
// Author: Ricardo Martins                                                  *
//***************************************************************************

// ISO C++ 98 headers.
#include <memory>
#include <cstring>
#include <algorithm>
#include <cerrno>
#include <cstdlib>

// DUNE headers.
#include <DUNE/DUNE.hpp>

// Local headers.
#include "Command.hpp"

namespace Supervisors
{
  namespace MobileInternet
  {
    using DUNE_NAMESPACES;

    struct Arguments
    {
      //! GSM username.
      std::string gsm_user;
      //! GSM password.
      std::string gsm_pass;
      //! GSM APN.
      std::string gsm_apn;
      //! GSM pin.
      std::string gsm_pin;
      //! GSM mode.
      std::string gsm_mode;
    };

    struct Task: public Tasks::Task
    {
      //! Task arguments.
      Arguments m_args;
      //! Command.
      Command m_cmd;

      Task(const std::string& name, Tasks::Context& ctx):
        Tasks::Task(name, ctx)
      {
        // Define configuration parameters.
        param("GSM - User", m_args.gsm_user)
        .defaultValue("vodafone")
        .description("GSM/GPRS username");

        param("GSM - Password", m_args.gsm_pass)
        .defaultValue("vodafone")
        .description("GSM/GPRS password");

        param("GSM - APN", m_args.gsm_apn)
        .defaultValue("internet.vodafone.pt")
        .description("GSM/GPRS Access Point Name (APN)");

        param("GSM - Pin", m_args.gsm_pin)
        .defaultValue("")
        .description("GSM/GPRS pin.");

        param("GSM - Mode", m_args.gsm_mode)
        .defaultValue("AT\\^SYSCFG=2,2,3fffffff,0,1")
        .description("GSM/GPRS mode.");
      }

      ~Task(void)
      {
        m_cmd.stop();
      }

      void
      connect(void)
      {
        std::string pin("AT");
        if (m_args.gsm_pin.size() == 4)
        {
          pin.append("+CPIN=");
          pin.append(m_args.gsm_pin);
        }

        Environment::set("GSM_USER", m_args.gsm_user);
        Environment::set("GSM_PASS", m_args.gsm_pass);
        Environment::set("GSM_APN", m_args.gsm_apn);
        Environment::set("GSM_PIN", pin);
        Environment::set("GSM_MODE", m_args.gsm_mode);

        m_cmd.start();
      }

      void
      onMain(void)
      {
        connect();

        while (!stopping())
        {
          waitForMessages(1.0);

          if (m_cmd.ended())
            m_cmd.start();
        }
      }
    };
  }
}

DUNE_TASK
