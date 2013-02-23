//***************************************************************************
// Copyright (C) 2007-2013 Laboratório de Sistemas e Tecnologia Subaquática *
// Departamento de Engenharia Electrotécnica e de Computadores              *
// Rua Dr. Roberto Frias, 4200-465 Porto, Portugal                          *
//***************************************************************************
// Author: Ricardo Martins                                                  *
//***************************************************************************

// ISO C++ 98 headers.
#include <cstring>

// DUNE headers.
#include <DUNE/DUNE.hpp>

namespace Sensors
{
  //! Device driver for the Imagenex 872 "YellowFin" Sidescan Sonar.
  namespace Imagenex872
  {
    using DUNE_NAMESPACES;

    enum Side
    {
      SIDE_PORT,
      SIDE_STARBOARD
    };

    enum Index
    {
      // Range index.
      SD_RANGE = 3,
      // Mode/Frequency index.
      SD_MODE_FREQ = 7,
      // Data Gain index.
      SD_DAT_GAIN = 8,
      // Balance Gain index.
      SD_BAL_GAIN = 10,
      // TCP Packet index.
      SD_TCP_PKT = 18
    };

    struct Arguments
    {
      // IPv4 address.
      Address addr;
      // TCP port.
      unsigned port;
      // Data gain.
      unsigned dat_gain;
      // Balance gain.
      unsigned bal_gain;
      // Default frequency.
      unsigned frequency;
      // Default range.
      unsigned range;
    };

    // List of available ranges.
    static const unsigned c_ranges[] = {10, 20, 30, 40, 50, 60, 80, 100, 125, 150, 200};
    // Count of available ranges.
    static const unsigned c_ranges_size = sizeof(c_ranges) / sizeof(c_ranges[0]);
    // List of range rates in ms.
    static const unsigned c_range_rates[] = {57, 57, 63, 76, 90, 102, 129, 156, 190, 223, 290};
    // List of available frequencies.
    static const unsigned c_freqs[] = {260, 330, 770};
    // Count of available frequencies.
    static const unsigned c_freqs_size = sizeof(c_freqs) / sizeof(c_freqs[0]);
    // List of beam width configurations.
    static const float c_beam_width[] = {75, 60, 30};
    // List of beam height configurations.
    static const float c_beam_height[] = {2.2, 1.8, 0.7};
    // Switch data size.
    static const int c_sdata_size = 27;
    // Return data header size.
    static const int c_rdata_hdr_size = 12;
    // Return data payload size.
    static const int c_rdata_dat_size = 1000;
    // Return data footer size.
    static const int c_rdata_ftr_size = 1;

    struct Task: public Tasks::Periodic
    {
      // TCP socket.
      TCPSocket* m_sock;
      // Output switch data.
      uint8_t m_sdata[c_sdata_size];
      // Return data.
      uint8_t m_rdata_hdr[c_rdata_hdr_size];
      // Return data.
      uint8_t m_rdata_ftr[c_rdata_ftr_size];
      // Single sidescan ping.
      IMC::SonarData m_ping;
      // True if sampling is active.
      bool m_active;
      // Configuration parameters.
      Arguments m_args;

      Task(const std::string& name, Tasks::Context& ctx):
        Tasks::Periodic(name, ctx),
        m_sock(NULL),
        m_active(false)
      {
        // Define configuration parameters.
        param("IPv4 Address", m_args.addr)
        .defaultValue("192.168.0.5")
        .description("IP address of the sonar");

        param("TCP Port", m_args.port)
        .defaultValue("4040")
        .description("TCP port");

        param("Data Gain", m_args.dat_gain)
        .defaultValue("40")
        .units(Units::Percentage)
        .minimumValue("0")
        .maximumValue("100")
        .description("Data gain");

        param("Balance Gain", m_args.bal_gain)
        .defaultValue("30")
        .minimumValue("0")
        .maximumValue("60")
        .description("Balance gain");

        param(DTR_RT("Frequency"), m_args.frequency)
        .visibility(Tasks::Parameter::VISIBILITY_USER)
        .scope(Tasks::Parameter::SCOPE_MANEUVER)
        .defaultValue("770")
        .values("260, 330, 770")
        .units(Units::Kilohertz)
        .description(DTR("Operating frequency"));

        param(DTR_RT("Range"), m_args.range)
        .visibility(Tasks::Parameter::VISIBILITY_USER)
        .scope(Tasks::Parameter::SCOPE_MANEUVER)
        .defaultValue("30")
        .units(Units::Meter)
        .valuesIf("Frequency", "260", "10, 20, 30, 40, 50, 60, 80, 100, 150, 200")
        .valuesIf("Frequency", "330", "10, 20, 30, 40, 50, 60, 80, 100, 150, 200")
        .valuesIf("Frequency", "770", "10, 20, 30, 40, 50")
        .description(DTR("Operating range"));

        // Initialize switch data.
        std::memset(m_sdata, 0, sizeof(m_sdata));
        m_sdata[0] = 0xfe;
        m_sdata[1] = 0x44;
        m_sdata[26] = 0xfd;

        // Initialize return data.
        m_ping.data.resize(c_rdata_dat_size * 2);
        m_ping.type = IMC::SonarData::ST_SIDESCAN;
        m_ping.bits_per_point = 8;
        m_ping.scale_factor = 1.0f;

        bind<IMC::EntityControl>(this);
        bind<IMC::SonarConfig>(this);
      }

      void
      onUpdateParameters(void)
      {
        if (paramChanged(m_args.frequency))
          setFrequency(m_args.frequency);

        if (paramChanged(m_args.range))
          setRange(m_args.range);

        if (paramChanged(m_args.dat_gain))
          setDataGain(m_args.dat_gain);

        if (paramChanged(m_args.bal_gain))
          setBalanceGain(m_args.bal_gain);

        if (paramChanged(m_args.addr))
        {
          if (m_sock)
            throw RestartNeeded(DTR("restarting to change IPv4 address"), 1);
        }

        if (paramChanged(m_args.port))
        {
          if (m_sock)
            throw RestartNeeded(DTR("restarting to change TCP port"), 1);
        }
      }

      void
      onResourceAcquisition(void)
      {
        m_sock = new TCPSocket();
      }

      void
      onResourceRelease(void)
      {
        Memory::clear(m_sock);
      }

      void
      onResourceInitialization(void)
      {
        try
        {
          m_sock->connect(m_args.addr, m_args.port);
          pingBoth();
          setEntityState(IMC::EntityState::ESTA_NORMAL, Status::CODE_IDLE);
        }
        catch (...)
        {
          setEntityState(IMC::EntityState::ESTA_ERROR, Status::CODE_COM_ERROR);
          throw;
        }
      }

      void
      consume(const IMC::EntityControl* msg)
      {
        if (msg->getDestinationEntity() != getEntityId())
          return;

        m_active = (msg->op == IMC::EntityControl::ECO_ACTIVATE);

        if (m_active)
          setEntityState(IMC::EntityState::ESTA_NORMAL, Status::CODE_ACTIVE);
        else
          setEntityState(IMC::EntityState::ESTA_NORMAL, Status::CODE_IDLE);
      }

      void
      consume(const IMC::SonarConfig* msg)
      {
        if (msg->getDestinationEntity() != getEntityId())
          return;

        setRange(msg->max_range);
        setFrequency(msg->frequency);
      }

      unsigned
      getIndex(unsigned value, const unsigned* table, unsigned table_size)
      {
        for (unsigned i = 0; i < table_size; ++i)
        {
          if (value == table[i])
            return i;
          else if (value < table[i])
            return (i == 0) ? 0 : i - 1;
        }

        return table_size - 1;
      }

      void
      setFrequency(unsigned value)
      {
        unsigned idx = getIndex(value, c_freqs, c_freqs_size);
        m_sdata[SD_MODE_FREQ] = (uint8_t)idx;
        m_ping.frequency = c_freqs[idx] * 1000;

        m_ping.beam_config.clear();
        IMC::BeamConfig bc;
        bc.beam_width = Math::Angles::radians(c_beam_width[idx]);
        bc.beam_height = Math::Angles::radians(c_beam_height[idx]);
        m_ping.beam_config.push_back(bc);
      }

      void
      setRange(unsigned value)
      {
        unsigned idx = getIndex(value, c_ranges, c_ranges_size);
        m_sdata[SD_RANGE] = (uint8_t)c_ranges[idx];
        m_ping.min_range = 0;
        m_ping.max_range = c_ranges[idx];
        Periodic::setFrequency(1.0 / (c_range_rates[idx] / 1000.0));
      }

      void
      setDataGain(unsigned value)
      {
        m_sdata[SD_DAT_GAIN] = (uint8_t) Math::trimValue(value, 0u, 100u);
      }

      void
      setBalanceGain(unsigned value)
      {
        m_sdata[SD_BAL_GAIN] = (uint8_t) Math::trimValue(value, 0u, 60u);
      }

      void
      ping(Side side)
      {
        m_sdata[SD_TCP_PKT] = (side == SIDE_STARBOARD) ? 0x02 : 0x00;
        m_sock->write((char*)m_sdata, c_sdata_size);

        int rv = m_sock->read((char*)m_rdata_hdr, c_rdata_hdr_size);
        if (rv != c_rdata_hdr_size)
          throw std::runtime_error(DTR("failed to read header"));

        unsigned dat_idx = ((side == SIDE_STARBOARD) ? 1 : 0) * c_rdata_dat_size;
        rv = m_sock->read(&m_ping.data[dat_idx], c_rdata_dat_size);
        if (rv != c_rdata_dat_size)
          throw std::runtime_error(DTR("failed to read data"));

        rv = m_sock->read((char*)m_rdata_ftr, c_rdata_ftr_size);
        if (rv != c_rdata_ftr_size)
          throw std::runtime_error(DTR("failed to read footer"));

        // Correct port imagery.
        if (side == SIDE_PORT)
        {
          for (unsigned i = 0; i < c_rdata_dat_size / 2; ++i)
          {
            char tmp = m_ping.data[i];
            m_ping.data[i] = m_ping.data[c_rdata_dat_size - 1 - i];
            m_ping.data[c_rdata_dat_size - 1 - i] = tmp;
          }
        }
      }

      void
      pingBoth(void)
      {
        ping(SIDE_PORT);
        ping(SIDE_STARBOARD);
        setEntityState(IMC::EntityState::ESTA_NORMAL, Status::CODE_ACTIVE);
      }

      void
      task(void)
      {
        if (!m_active)
          return;

        try
        {
          pingBoth();
          dispatch(m_ping);
        }
        catch (std::exception& e)
        {
          err("%s", e.what());
          setEntityState(IMC::EntityState::ESTA_ERROR, Status::CODE_COM_ERROR);
        }
      }
    };
  }
}

DUNE_TASK