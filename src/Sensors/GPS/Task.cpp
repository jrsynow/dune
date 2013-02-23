//***************************************************************************
// Copyright (C) 2007-2013 Laboratório de Sistemas e Tecnologia Subaquática *
// Departamento de Engenharia Electrotécnica e de Computadores              *
// Rua Dr. Roberto Frias, 4200-465 Porto, Portugal                          *
//***************************************************************************
// Author: Ricardo Martins                                                  *
//***************************************************************************

// ISO C++ 98 headers.
#include <cstring>
#include <algorithm>
#include <cstddef>

// DUNE headers.
#include <DUNE/DUNE.hpp>

namespace Sensors
{
  //! Device driver for NMEA capable %GPS devices.
  namespace GPS
  {
    using DUNE_NAMESPACES;

    //! Maximum number of initialization commands.
    static const unsigned c_max_init_cmds = 14;
    //! Timeout for waitReply() function.
    static const float c_wait_reply_tout = 4.0;
    //! Minimum number of fields of PUBX,00 sentence.
    static const unsigned c_pubx00_fields = 21;
    //! Minimum number of fields of GPGGA sentence.
    static const unsigned c_gpgga_fields = 15;
    //! Minimum number of fields of GPVTG sentence.
    static const unsigned c_gpvtg_fields = 9;
    //! Minimum number of fields of GPZDA sentence.
    static const unsigned c_gpzda_fields = 7;
    //! Minimum number of fields of GPHDT sentence.
    static const unsigned c_gphdt_fields = 3;
    //! Minimum number of fields of GPHDM sentence.
    static const unsigned c_gphdm_fields = 3;
    //! Minimum number of fields of GPROT sentence.
    static const unsigned c_gprot_fields = 3;
    //! Minimum number of fields of PSATHPR sentence.
    static const unsigned c_psathpr_fields = 7;

    struct Arguments
    {
      //! Serial port device.
      std::string uart_dev;
      //! Serial port baud rate.
      unsigned uart_baud;
      //! Order of sentences.
      std::vector<std::string> stn_order;
      //! Input timeout in seconds.
      float inp_tout;
      //! Initialization commands.
      std::string init_cmds[c_max_init_cmds];
      //! Initialization replies.
      std::string init_rpls[c_max_init_cmds];
    };

    struct Task: public Tasks::Task
    {
      //! Serial port handle.
      SerialPort* m_uart;
      //! GPS Fix message.
      IMC::GpsFix m_fix;
      //! Euler angles message.
      IMC::EulerAngles m_euler;
      //! Angular velocity message.
      IMC::AngularVelocity m_agvel;
      //! Task arguments.
      Arguments m_args;
      //! Input watchdog.
      Time::Counter<float> m_wdog;
      //! True if we have angular velocity.
      bool m_has_agvel;
      //! True if we have euler angles.
      bool m_has_euler;

      Task(const std::string& name, Tasks::Context& ctx):
        Tasks::Task(name, ctx),
        m_uart(NULL),
        m_has_agvel(false),
        m_has_euler(false)
      {
        // Define configuration parameters.
        param("Serial Port - Device", m_args.uart_dev)
        .defaultValue("")
        .description("Serial port device used to communicate with the sensor");

        param("Serial Port - Baud Rate", m_args.uart_baud)
        .defaultValue("4800")
        .description("Serial port baud rate");

        param("Input Timeout", m_args.inp_tout)
        .units(Units::Second)
        .defaultValue("4.0")
        .description("Input timeout");

        param("Sentence Order", m_args.stn_order)
        .defaultValue("")
        .description("Sentence order");

        for (unsigned i = 0; i < c_max_init_cmds; ++i)
        {
          std::string cmd_label = String::str("Initialization String %u - Command", i);
          param(cmd_label, m_args.init_cmds[i])
          .defaultValue("");

          std::string rpl_label = String::str("Initialization String %u - Reply", i);
          param(rpl_label, m_args.init_rpls[i])
          .defaultValue("");
        }

        // Initialize messages.
        clear();
      }

      ~Task(void)
      {
        Task::onResourceRelease();
      }

      void
      clear(void)
      {
        m_euler.clear();
        m_agvel.clear();
        m_fix.clear();
      }

      void
      onResourceAcquisition(void)
      {
        try
        {
          m_uart = new SerialPort(m_args.uart_dev, m_args.uart_baud);
          m_uart->setCanonicalInput(true);
        }
        catch (...)
        {
          throw RestartNeeded(DTR(Status::getString(CODE_COM_ERROR)), 5);
        }
      }

      void
      onResourceRelease(void)
      {
        Memory::clear(m_uart);
      }

      void
      onResourceInitialization(void)
      {
        for (unsigned i = 0; i < c_max_init_cmds; ++i)
        {
          if (m_args.init_cmds[i].empty())
            continue;

          std::string cmd = String::unescape(m_args.init_cmds[i]);
          m_uart->write(cmd.c_str());

          if (!m_args.init_rpls[i].empty())
          {
            std::string rpl = String::unescape(m_args.init_rpls[i]);
            if (!waitReply(rpl))
            {
              err("%s: %s", DTR("no reply to command"), m_args.init_cmds[i].c_str());
              throw std::runtime_error(DTR("failed to setup device"));
            }
          }
        }

        m_wdog.setTop(m_args.inp_tout);
      }

      //! Wait reply
      //! @param[in] stn string to compare
      //! @return true if successful match, false otherwise.
      bool
      waitReply(const std::string& stn)
      {
        char line[256];
        Counter<float> counter(c_wait_reply_tout);

        while (!stopping() && !counter.overflow())
        {
          consumeMessages();

          if (m_uart->hasNewData(0.5) != IOMultiplexing::PRES_OK)
            continue;

          int rv = m_uart->readString(line, sizeof(line));
          if (rv == 0)
            continue;

          if (stn.compare(line) == 0)
            return true;
        }

        return false;
      }

      //! Read time from string.
      //! @param[in] str string.
      //! @param[out] dst time.
      //! @return true if successful, false otherwise.
      bool
      readTime(const std::string& str, float& dst)
      {
        unsigned h = 0;
        unsigned m = 0;
        unsigned s = 0;
        double sfp = 0;

        if (std::sscanf(str.c_str(), "%02u%02u%lf", &h, &m, &sfp) != 3)
        {
          if (std::sscanf(str.c_str(), "%02u%02u%02u", &h, &m, &s) != 3)
            return false;
        }

        dst = (h * 3600) + (m * 60) + s + sfp;

        return true;
      }

      //! Read latitude from string.
      //! @param[in] str input string.
      //! @param[in] h either North (N) or South (S).
      //! @param[out] dst latitude.
      //! @return true if successful, false otherwise.
      bool
      readLatitude(const std::string& str, const std::string& h, double& dst)
      {
        int degrees = 0;
        double minutes = 0;

        if (std::sscanf(str.c_str(), "%02d%lf", &degrees, &minutes) != 2)
          return false;

        dst = Angles::convertDMSToDecimal(degrees, minutes);

        if (h == "S")
          dst = -dst;

        return true;
      }

      //! Read longitude from string.
      //! @param[in] str input string.
      //! @param[in] h either West (W) or East (E).
      //! @param[out] dst longitude.
      //! @return true if successful, false otherwise.
      double
      readLongitude(const std::string& str, const std::string& h, double& dst)
      {
        int degrees = 0;
        double minutes = 0;

        if (std::sscanf(str.c_str(), "%03d%lf", &degrees, &minutes) != 2)
          return false;

        dst = Angles::convertDMSToDecimal(degrees, minutes);

        if (h == "W")
          dst = -dst;

        return true;
      }

      //! Read decimal from input string.
      //! @param[in] str input string.
      //! @param[out] dst decimal.
      //! @return true if successful, false otherwise.
      template <typename T>
      bool
      readDecimal(const std::string& str, T& dst)
      {
        unsigned idx = 0;
        while (str[idx] == '0')
          ++idx;

        return castLexical(std::string(str, idx), dst);
      }

      //! Read number from input string.
      //! @param[in] str input string.
      //! @param[out] dst number.
      //! @return true if successful, false otherwise.
      template <typename T>
      bool
      readNumber(const std::string& str, T& dst)
      {
        return castLexical(str, dst);
      }

      //! Process sentence.
      //! @param[in] line pointer to sentence.
      //! @param[in] line_len length of sentence.
      void
      processSentence(char* line, int line_len)
      {
        // Discard leading noise.
        int sidx = 0;
        for (sidx = 0; sidx < line_len; ++sidx)
        {
          if (line[sidx] == '$')
            break;
        }

        // Discard trailing noise.
        int eidx = 0;
        for (eidx = line_len - 1; eidx > sidx; --eidx)
        {
          if (line[eidx] == '*')
            break;
        }

        // Compute checksum.
        uint8_t ccsum = 0;
        for (int i = sidx + 1; i < eidx; ++i)
          ccsum ^= line[i];

        // Validate checksum.
        unsigned rcsum = 0;
        if (std::sscanf(line + eidx + 1, "%02X", &rcsum) != 1)
          return;

        // Remove checksum from sentence.
        line[eidx] = 0;

        // Split sentence
        std::vector<std::string> parts;
        String::split(line + sidx + 1, ",", parts);

        if (std::find(m_args.stn_order.begin(), m_args.stn_order.end(), parts[0]) != m_args.stn_order.end())
          interpretSentence(parts);
      }

      //! Interpret given sentence.
      //! @param[in] parts vector of strings from sentence.
      void
      interpretSentence(std::vector<std::string>& parts)
      {
        if (parts[0] == m_args.stn_order.front())
        {
          clear();
          m_fix.setTimeStamp();
          m_euler.setTimeStamp(m_fix.getTimeStamp());
          m_agvel.setTimeStamp(m_fix.getTimeStamp());
          m_wdog.reset();
        }

        if (parts[0] == "GPZDA")
        {
          interpretGPZDA(parts);
        }
        else if (parts[0] == "GPGGA")
        {
          interpretGPGGA(parts);
        }
        else if (parts[0] == "GPVTG")
        {
          interpretGPVTG(parts);
        }
        else if (parts[0] == "PSAT")
        {
          if (parts[1] == "HPR")
            interpretPSATHPR(parts);
        }
        else if (parts[0] == "PUBX")
        {
          if (parts[1] == "00")
            interpretPUBX00(parts);
        }
        else if (parts[0] == "GPHDM")
        {
          interpretGPHDM(parts);
        }
        else if (parts[0] == "GPHDT")
        {
          interpretGPHDT(parts);
        }
        else if (parts[0] == "GPROT")
        {
          interpretGPROT(parts);
        }

        if (parts[0] == m_args.stn_order.back())
        {
          dispatch(m_fix);
          if (m_has_euler)
          {
            dispatch(m_euler);
            m_has_euler = false;
          }

          if (m_has_agvel)
          {
            dispatch(m_agvel);
            m_has_agvel = false;
          }

          if (m_fix.validity & IMC::GpsFix::GFV_VALID_POS)
            setEntityState(IMC::EntityState::ESTA_NORMAL, Status::CODE_ACTIVE);
          else
            setEntityState(IMC::EntityState::ESTA_NORMAL, Status::CODE_WAIT_GPS_FIX);
        }
      }

      //! Interpret GPZDA sentence (UTC date and time).
      //! @param[in] parts vector of strings from sentence.
      void
      interpretGPZDA(const std::vector<std::string>& parts)
      {
        if (parts.size() < c_gpzda_fields)
        {
          war(DTR("invalid GPZDA sentence"));
          return;
        }

        // Read time.
        if (readTime(parts[1], m_fix.utc_time))
          m_fix.validity |= IMC::GpsFix::GFV_VALID_TIME;

        // Read date.
        if (readDecimal(parts[2], m_fix.utc_day)
            && readDecimal(parts[3], m_fix.utc_month)
            && readDecimal(parts[4], m_fix.utc_year))
        {
          m_fix.validity |= IMC::GpsFix::GFV_VALID_DATE;
        }
      }

      //! Interpret GPGGA sentence (GPS fix data).
      //! @param[in] parts vector of strings from sentence.
      void
      interpretGPGGA(const std::vector<std::string>& parts)
      {
        if (parts.size() < c_gpgga_fields)
        {
          war(DTR("invalid GPGGA sentence"));
          return;
        }

        if (readLatitude(parts[2], parts[3], m_fix.lat)
            && readLongitude(parts[4], parts[5], m_fix.lon)
            && readNumber(parts[9], m_fix.height)
            && readDecimal(parts[7], m_fix.satellites))
        {
          // Convert altitude above sea level to altitude above ellipsoid.
          double geoid_sep = 0;
          if (readNumber(parts[11], geoid_sep))
            m_fix.height += geoid_sep;

          // Convert coordinates to radians.
          m_fix.lat = Angles::radians(m_fix.lat);
          m_fix.lon = Angles::radians(m_fix.lon);
          m_fix.validity |= IMC::GpsFix::GFV_VALID_POS;
        }

        int quality = 0;
        readDecimal(parts[6], quality);
        if (quality == 1)
        {
          m_fix.type = IMC::GpsFix::GFT_STANDALONE;
          m_fix.validity |= IMC::GpsFix::GFV_VALID_POS;
        }
        else if (quality == 2)
        {
          m_fix.type = IMC::GpsFix::GFT_DIFFERENTIAL;
          m_fix.validity |= IMC::GpsFix::GFV_VALID_POS;
        }
        else
        {
          m_fix.validity &= ~IMC::GpsFix::GFV_VALID_POS;
        }

        if (readNumber(parts[8], m_fix.hdop))
          m_fix.validity |= IMC::GpsFix::GFV_VALID_HDOP;
      }

      //! Interpret PUBX00 sentence (navstar position).
      //! @param[in] parts vector of strings from sentence.
      void
      interpretPUBX00(const std::vector<std::string>& parts)
      {
        if (parts.size() < c_pubx00_fields)
        {
          war(DTR("invalid PUBX,00 sentence"));
          return;
        }

        if (readLatitude(parts[3], parts[4], m_fix.lat)
            && readLongitude(parts[5], parts[6], m_fix.lon)
            && readNumber(parts[7], m_fix.height)
            && readDecimal(parts[18], m_fix.satellites))
        {
          // Convert coordinates to radians.
          m_fix.lat = Angles::radians(m_fix.lat);
          m_fix.lon = Angles::radians(m_fix.lon);

          m_fix.validity |= IMC::GpsFix::GFV_VALID_POS;
        }

        if (parts[8] == "G3" || parts[8] == "G2")
        {
          m_fix.type = IMC::GpsFix::GFT_STANDALONE;
          m_fix.validity |= IMC::GpsFix::GFV_VALID_POS;
        }
        else if (parts[8] == "D3" || parts[8] == "D2")
        {
          m_fix.type = IMC::GpsFix::GFT_DIFFERENTIAL;
          m_fix.validity |= IMC::GpsFix::GFV_VALID_POS;
        }
        else
        {
          m_fix.validity &= ~IMC::GpsFix::GFV_VALID_POS;
        }

        if (readNumber(parts[9], m_fix.hacc))
          m_fix.validity |= IMC::GpsFix::GFV_VALID_HACC;

        if (readNumber(parts[10], m_fix.vacc))
          m_fix.validity |= IMC::GpsFix::GFV_VALID_VACC;

        if (readNumber(parts[15], m_fix.hdop))
          m_fix.validity |= IMC::GpsFix::GFV_VALID_HDOP;

        if (readNumber(parts[16], m_fix.vdop))
          m_fix.validity |= IMC::GpsFix::GFV_VALID_VDOP;
      }

      //! Interpret GPVTG sentence (course over ground).
      //! @param[in] parts vector of strings from sentence.
      void
      interpretGPVTG(const std::vector<std::string>& parts)
      {
        if (parts.size() < c_gpvtg_fields)
        {
          war(DTR("invalid GPVTG sentence"));
          return;
        }

        if (readNumber(parts[1], m_fix.cog))
        {
          m_fix.cog = Angles::normalizeRadian(Angles::radians(m_fix.cog));
          m_fix.validity |= IMC::GpsFix::GFV_VALID_COG;
        }

        if (readNumber(parts[7], m_fix.sog))
        {
          m_fix.sog *= 1000.0f / 3600.0f;
          m_fix.validity |= IMC::GpsFix::GFV_VALID_SOG;
        }
      }

      //! Interpret GPVTG sentence (true heading).
      //! @param[in] parts vector of strings from sentence.
      void
      interpretGPHDT(const std::vector<std::string>& parts)
      {
        if (parts.size() < c_gphdt_fields)
        {
          war(DTR("invalid GPHDT sentence"));
          return;
        }

        if (readNumber(parts[1], m_euler.psi))
          m_euler.psi = Angles::normalizeRadian(Angles::radians(m_euler.psi));
      }

      //! Interpret GPHDM sentence (Magnetic heading of
      //! the vessel derived from the true heading calculated).
      //! @param[in] parts vector of strings from sentence.
      void
      interpretGPHDM(const std::vector<std::string>& parts)
      {
        if (parts.size() < c_gphdm_fields)
        {
          war(DTR("invalid GPHDM sentence"));
          return;
        }

        if (readNumber(parts[1], m_euler.psi_magnetic))
        {
          m_euler.psi_magnetic = Angles::normalizeRadian(Angles::radians(m_euler.psi_magnetic));
          m_has_euler = true;
        }
      }

      //! Interpret GPROT sentence (rate of turn).
      //! @param[in] parts vector of strings from sentence.
      void
      interpretGPROT(const std::vector<std::string>& parts)
      {
        if (parts.size() < c_gprot_fields)
        {
          war(DTR("invalid GPROT sentence"));
          return;
        }

        if (readNumber(parts[1], m_agvel.z))
        {
          m_agvel.z = Angles::radians(m_agvel.z) / 60.0;
          m_has_agvel = true;
        }
      }

      //! Interpret PSATHPR sentence (Proprietary NMEA message that
      //! provides the heading, pitch, roll, and time in a single message).
      //! @param[in] parts vector of strings from sentence.
      void
      interpretPSATHPR(const std::vector<std::string>& parts)
      {
        if (parts.size() < c_psathpr_fields)
        {
          war(DTR("invalid PSATHPR sentence"));
          return;
        }

        if (readNumber(parts[4], m_euler.theta))
        {
          m_euler.theta = Angles::normalizeRadian(Angles::radians(m_euler.theta));
          m_has_euler = true;
        }

        if (readNumber(parts[5], m_euler.phi))
        {
          m_euler.phi = Angles::normalizeRadian(Angles::radians(m_euler.phi));
          m_has_euler = true;
        }
      }

      void
      onMain(void)
      {
        char line[512];

        while (!stopping())
        {
          consumeMessages();

          if (m_uart->hasNewData(0.5) == IOMultiplexing::PRES_OK)
          {
            int rv = m_uart->readString(line, sizeof(line));

            if (rv <= 0)
            {
              throw RestartNeeded(DTR(Status::getString(CODE_COM_ERROR)), 5);
            }
            else
            {
              processSentence(line, rv);
            }
          }

          if (m_wdog.overflow())
            setEntityState(IMC::EntityState::ESTA_ERROR, Status::CODE_COM_ERROR);
        }
      }
    };
  }
}

DUNE_TASK