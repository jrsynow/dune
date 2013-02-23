//***************************************************************************
// Copyright (C) 2007-2013 Laboratório de Sistemas e Tecnologia Subaquática *
// Departamento de Engenharia Electrotécnica e de Computadores              *
// Rua Dr. Roberto Frias, 4200-465 Porto, Portugal                          *
//***************************************************************************
// Author: José Braga                                                       *
//***************************************************************************

// ISO C++ 98 headers.
#include <cstring>

// DUNE headers.
#include <DUNE/DUNE.hpp>

// Local headers.
#include "Frame.hpp"

namespace Sensors
{
  //! Device driver for the Imagenex 837B “Delta T” Multibeam
  //! Profiling Sonar.
  //!
  //! The Model 837 DeltaT is an advanced high-speed, high-resolution
  //! multibeam sonar system that has been designed to provide
  //! simple, reliable, and accurate representation of underwater images.
  //!
  //! This device driver is capable of controlling the following
  //! parameters:
  //!  - <em>Range</em>: Range of the multibeam. Unit is meter,
  //!    the default value is 30, and the domain is {5, 10, 20,
  //!    30, 40, 50, 60, 80, 100}
  //!  - <em>Start Gain</em>: Unit is Decibel, the default value
  //!  - is 3 and the domain is {0, 1, 2, ..., 20}.
  //!  - <em>Absorption</em>: Unit is Decibel per meter, the
  //!    default value is 0.1 and the domain is [0.00, 2.55]
  //!  - <em>Data Points</em>: Number of sonar return data points
  //!     -# <em>8</em>: 8000 data points are returned by the head
  //!     -# <em>16</em>: 16000 data points are returned by the head
  //!  - <em>Switch Delay</em>: The head can be commanded to pause
  //!    before sending its return data to allow the commanding
  //!    program enough time to setup for the return of the data.
  //!    Unit is millisecond, default value is 0 and the domain is
  //!    [0, 500].
  //!    <em>Auto Gain</em>: Auto Gain, set to 1 to enable Automatic
  //!    Gain Control. If the sonar head transducer is pointing at an
  //!    angle other than straight down, the mounting angle and/or
  //!    the roll angle must be loaded into Nadir Offset Angle
  //!    An Automatic Gain Control threshold value must also be loaded.
  //!  - <em>Automatic Gain Control</em>: When using Automatic Gain
  //!    Control, this number is used as a set point for adjusting
  //!    the internal hardware gain. For strong bottom returns, use
  //!    a low threshold value. For weak bottom returns, use a high
  //!    threshold value. A value of 120 is a typical threshold value
  //!    for a sandy bottom. Unitless, the default value is 120 and the
  //!    domain is [10, 250]
  //!  - <em>Nadir Offset Angle</em>: When using Automatic Gain Control,
  //!    the sonar head must know if there is a physical mounting offset
  //!    and/or a roll angle present.
  //!
  //! This driver will output raw data from the sonar for each measurement.
  //!
  //! @author José Braga
  namespace Imagenex837B
  {
    using DUNE_NAMESPACES;

    //! Necessary bytes for switch data command.
    enum Index
    {
      // Range index.
      SD_RANGE = 3,
      // Nadir Offset Angle High Byte index.
      SD_NADIR_HI = 5,
      // Nadir Offset Angle Low Byte index.
      SD_NADIR_LO = 6,
      // Start Gain index.
      SD_START_GAIN = 8,
      // Absorption index.
      SD_ABSORPTION = 10,
      // Automatic Gain Control Threshold index.
      SD_AGC_THRESHOLD = 11,
      // Packet Number index.
      SD_PACKET_NUM = 13,
      // Pulse length index.
      SD_PULSE_LEN = 14,
      // Data points index.
      SD_DATA_POINTS = 19,
      // Run Mode index.
      SD_RUN_MODE = 22,
      // Switch Delay index.
      SD_SWITCH_DELAY = 24,
      // Frequency index.
      SD_FREQUENCY = 25
    };

    //! %Task arguments.
    struct Arguments
    {
      //! IPv4 address.
      Address addr;
      //! TCP port.
      unsigned port;
      //! Start gain.
      unsigned start_gain;
      //! Absorption.
      float absorption;
      //! Data points.
      unsigned data_points;
      //! Switch delay.
      unsigned switch_delay;
      //! Default range.
      unsigned def_range;
      //! Nadir Offset Angle.
      float nadir;
      //! Auto gain mode.
      bool auto_gain;
      //! Automatic gain control threshold.
      unsigned auto_gain_value;
      //! Transducer mounting position.
      bool xdcr;
      //! Save data in 837 format.
      bool save_in_837;
      //! Save data in 837 format.
      bool fill_state;
    };

    //! List of available ranges.
    static const unsigned c_ranges[] = {5, 10, 20, 30, 40, 50, 60, 80, 100};
    //! Count of available ranges.
    static const unsigned c_ranges_size = sizeof(c_ranges) / sizeof(c_ranges[0]);
    //! List of repetition rates in ms.
    static const unsigned c_rep_rate[] = {67, 73, 87, 100, 114, 128, 140, 167, 195};
    //! List of pulse lengths in us.
    static const unsigned c_pulse_len[] = {30, 60, 120, 180, 240, 300, 360, 480, 600};
    //! Switch data size.
    static const int c_sdata_size = 27;
    //! Return data header size.
    static const int c_rdata_hdr_size = 32;
    //! Return data payload size.
    static const int c_rdata_dat_size = 1000;
    //! Return data footer size.
    static const int c_rdata_ftr_size = 1;
    //! Delta T operating frequency.
    static const uint32_t c_freq = 260000;
    //! Delta T beam width.
    static const float c_beam_width = 3.0;
    //! Delta T beam height.
    static const float c_beam_height = 120.0;

    //! %Task.
    struct Task: public Tasks::Periodic
    {
      //! TCP socket.
      TCPSocket m_sock;
      //! 837 Frame.
      Frame m_frame;
      //! Output switch data.
      uint8_t m_sdata[c_sdata_size];
      //! Header Return data.
      uint8_t m_rdata_hdr[c_rdata_hdr_size];
      //! Footer Return data.
      uint8_t m_rdata_ftr[c_rdata_ftr_size];
      //! Single sidescan ping.
      IMC::SonarData m_ping;
      //! Log file name.
      Path m_log_file_name;
      //! Log file.
      std::ofstream m_log_file;
      //! True if sampling is active.
      bool m_active;
      //! Configuration parameters.
      Arguments m_args;

      //! Constructor.
      Task(const std::string& name, Tasks::Context& ctx):
        Tasks::Periodic(name, ctx),
        m_active(false)
      {
        // Define configuration parameters.
        param("IPv4 Address", m_args.addr)
        .defaultValue("192.168.0.2")
        .description("IP address of the sonar");

        param("TCP Port", m_args.port)
        .defaultValue("4040")
        .description("TCP port");

        param("Start Gain", m_args.start_gain)
        .defaultValue("3")
        .units(Units::Decibel)
        .minimumValue("0")
        .maximumValue("20")
        .description("Start gain");

        param("Absorption", m_args.absorption)
        .defaultValue("0.1")
        .units(Units::DecibelPerMeter)
        .minimumValue("0")
        .maximumValue("2.55")
        .description("Absorption");

        param("Data Points", m_args.data_points)
        .defaultValue("8000")
        .values("8000, 16000")
        .description("Number of sonar return data points");

        param("Switch Delay", m_args.switch_delay)
        .defaultValue("0")
        .units(Units::Millisecond)
        .minimumValue("0")
        .maximumValue("500")
        .description("The head can be commanded to pause before sending"
                     "its return data to allow the commanding program"
                     "enough time to setup for the return of the data.");

        param("Default Range", m_args.def_range)
        .defaultValue("30")
        .units(Units::Meter)
        .description("Default range");

        param("Nadir Offset Angle", m_args.nadir)
        .defaultValue("0.0")
        .units(Units::Degree)
        .minimumValue("-360")
        .maximumValue("360")
        .description("When using Automatic Gain Control (Byte 22, Bit 4)"
                     "the sonar head must know if there is a physical"
                     "mounting offset and/or a roll angle present.");

        param("Auto Gain Mode", m_args.auto_gain)
        .defaultValue("true")
        .description("Auto Gain, set to 1 to enable Automatic Gain Control."
                     "If the sonar head transducer is pointing at an angle "
                     "other than straight down, the mounting angle and/or "
                     "the roll angle must be loaded into Nadir Offset Angle"
                     "(see description for Bytes 5-6). An AGC Threshold"
                     "value must also be loaded into Byte 11.");

        param("Automatic Gain Control", m_args.auto_gain_value)
        .defaultValue("120")
        .minimumValue("10")
        .maximumValue("250")
        .description("Set point for adjusting the internal hardware gain."
                     "For strong bottom returns, use a low threshold value."
                     "For weak bottom returns, use a high threshold value."
                     "A value of 120 is a typical threshold value for a sandy bottom.");

        param("Connector Pointing Aft", m_args.xdcr)
        .defaultValue("true")
        .description("Mounting position of the multibeam");

        param("Save Data in 837 Format", m_args.save_in_837)
        .defaultValue("true")
        .description("Save multibeam in Imagenex proprietary 837 format");

        param("Fill State in 837 Format", m_args.fill_state)
        .defaultValue("true")
        .description("Fill state data in Imagenex proprietary 837 format");

        // Initialize switch data.
        std::memset(m_sdata, 0, sizeof(m_sdata));
        m_sdata[0] = 0xfe;
        m_sdata[1] = 0x44;
        m_sdata[2] = 0x10;
        m_sdata[9] = 0x01;
        m_sdata[20] = 0x08;
        m_sdata[26] = 0xfd;

        IMC::BeamConfig bc;
        bc.beam_width = Math::Angles::radians(c_beam_width);
        bc.beam_height = Math::Angles::radians(c_beam_height);
        m_ping.beam_config.clear();
        m_ping.beam_config.push_back(bc);

        // Register consumers.
        bind<IMC::EntityControl>(this);
        bind<IMC::LoggingControl>(this);
        bind<IMC::SonarConfig>(this);
        bind<IMC::SoundSpeed>(this);
      }

      //! Update task parameters.
      void
      onUpdateParameters(void)
      {
        m_args.data_points /= 1000;
        m_ping.data.resize(c_rdata_dat_size * m_args.data_points);

        if (m_args.fill_state)
          bind<IMC::EstimatedState>(this);
      }

      //! Initialize resources.
      void
      onResourceInitialization(void)
      {
        // Initialize return data.
        m_ping.type = IMC::SonarData::ST_MULTIBEAM;
        m_ping.bits_per_point = 8;
        m_ping.scale_factor = 1.0f;
        m_ping.min_range = 0;

        // Set switch command.
        setFrequency();
        setRange(m_args.def_range);
        setStartGain(m_args.start_gain);
        setSwitchDelay(m_args.switch_delay);
        setAbsorption((unsigned)(m_args.absorption * 100));
        setDataPoints(m_args.data_points);

        if (!m_args.auto_gain)
        {
          setAutoMode();
          setNadirAngle(m_args.nadir);
          setAutoGainValue(m_args.auto_gain_value);
        }

        try
        {
          m_sock.connect(m_args.addr, m_args.port);
          for (unsigned i = 0; i < m_args.data_points; ++i)
            ping(i);
          setEntityState(IMC::EntityState::ESTA_NORMAL, Status::CODE_ACTIVE);
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
      consume(const IMC::EstimatedState* msg)
      {
        setNadirAngle(m_args.nadir + Angles::degrees(msg->phi));

        double lat, lon;
        Coordinates::toWGS84(*msg, lat, lon);
        m_frame.setGpsData(lat, lon);
        m_frame.setSpeed(msg->u);
        m_frame.setCourse(msg->psi);
        m_frame.setRoll(msg->phi);
        m_frame.setPitch(msg->theta);
        m_frame.setHeading(msg->psi);
      }

      void
      consume(const IMC::LoggingControl* msg)
      {
        if (msg->getSource() != getSystemId())
          return;

        if (!m_args.save_in_837)
          return;

        switch (msg->op)
        {
          case IMC::LoggingControl::COP_STARTED:
            if (m_log_file.is_open())
              m_log_file.close();

            m_log_file.open((m_ctx.dir_log / msg->name / "Data.837").c_str(), std::ios::binary);
            break;
          case IMC::LoggingControl::COP_REQUEST_STOP:
            m_log_file.close();
            break;
        }
      }

      void
      consume(const IMC::SonarConfig* msg)
      {
        if (msg->getDestinationEntity() != getEntityId())
          return;

        setRange(msg->max_range);
      }

      void
      consume(const IMC::SoundSpeed* msg)
      {
        m_frame.setSoundVelocity((uint16_t)msg->value);
      }

      //! Get index from table according with given value.
      //! @param[in] value value to be checked in the table.
      //! @param[in] table vector with parameters.
      //! @param[in] table_size size of the vector.
      //! @return index of the vector.
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

      //! Define switch command data frequency value.
      void
      setFrequency(void)
      {
        m_sdata[SD_FREQUENCY] = (uint8_t)86;
        m_ping.frequency = c_freq;
      }

      //! Define switch command data range.
      //! @param[in] value range.
      void
      setRange(unsigned value)
      {
        unsigned idx = getIndex(value, c_ranges, c_ranges_size);

        m_sdata[SD_RANGE] = (uint8_t)c_ranges[idx];
        m_sdata[SD_PULSE_LEN] = (uint8_t)(c_pulse_len[idx] / 10);

        m_frame.setRange((uint8_t)c_ranges[idx]);
        m_frame.setPulseLength((uint8_t)(c_pulse_len[idx] / 10));

        m_ping.max_range = c_ranges[idx];
        Periodic::setFrequency(1.0 / (c_rep_rate[idx] / 1000.0));
      }

      //! Define switch command data start gain.
      //! @param[in] value start gain.
      void
      setStartGain(unsigned value)
      {
        m_sdata[SD_START_GAIN] = (uint8_t) Math::trimValue(value, 0u, 20u);
        m_frame.setStartGain(value);
      }

      //! Define switch command data switch delay.
      //! @param[in] value switch delay.
      void
      setSwitchDelay(unsigned value)
      {
        m_sdata[SD_SWITCH_DELAY] = (uint8_t) (Math::trimValue(value, 0u, 500u) / 2);
      }

      //! Define switch command data absorption value.
      //! @param[in] value absorption value.
      void
      setAbsorption(unsigned value)
      {
        m_sdata[SD_ABSORPTION] = (uint8_t) Math::trimValue(value, 0u, 255u);
      }

      //! Define switch command data number of data points.
      //! @param[in] value number of data points.
      void
      setDataPoints(unsigned value)
      {
        m_sdata[SD_DATA_POINTS] = (uint8_t)value;

        if (value == 16)
          m_frame.setExtendedDataPoints(true);
        if (value == 8)
          m_frame.setExtendedDataPoints(false);
      }

      //! Define switch command data auto mode.
      void
      setAutoMode(void)
      {
        m_sdata[SD_RUN_MODE] |= 0x10;
      }

      //! Define switch command data number nadir angle.
      //! @param[in] value nadir angle.
      void
      setNadirAngle(float angle)
      {
        if (m_args.xdcr)
          angle = -angle;

        uint16_t value = (uint16_t)(std::abs(angle) * 65535 / 360);

        if (angle < 0)
          value |= 0x8000;

        m_sdata[SD_NADIR_HI] = (uint8_t)((value & 0xff00) >> 8);
        m_sdata[SD_NADIR_LO] = (uint8_t)(value & 0x00ff);

        m_frame.setDisplayMode(m_args.xdcr);
      }

      //! Define switch command data auto gain threshold.
      //! @param[in] value auto gain threshold.
      void
      setAutoGainValue(unsigned value)
      {
        m_sdata[SD_AGC_THRESHOLD] = (uint8_t)value;
      }

      //! Request device to ping.
      //! @param[in] data_point data index.
      void
      ping(unsigned data_point)
      {
        m_sdata[SD_PACKET_NUM] = (uint8_t)data_point;
        m_sock.write((char*)m_sdata, c_sdata_size);

        int rv = m_sock.read((char*)m_rdata_hdr, c_rdata_hdr_size);
        if (rv != c_rdata_hdr_size)
          throw std::runtime_error(DTR("failed to read header"));

        unsigned dat_idx = data_point * c_rdata_dat_size;

        if (m_args.save_in_837)
          rv = m_sock.read((char*)(m_frame.getMessageData() + dat_idx), c_rdata_dat_size);
        else
          rv = m_sock.read(&m_ping.data[dat_idx], c_rdata_dat_size);

        if (rv != c_rdata_dat_size)
          throw std::runtime_error(DTR("failed to read data"));

        rv = m_sock.read((char*)m_rdata_ftr, c_rdata_ftr_size);
        if (rv != c_rdata_ftr_size)
          throw std::runtime_error(DTR("failed to read footer"));

        setEntityState(IMC::EntityState::ESTA_NORMAL, Status::CODE_ACTIVE);
      }

      void
      handleSonarData(void)
      {
        // Update information.
        m_frame.setDateTime();
        m_frame.setSerialStatus(m_rdata_hdr[4]);
        m_frame.setFirmwareVersion(m_rdata_hdr[6]);

        m_log_file.write((const char*)m_frame.getData(), m_frame.getSize());
      }

      void
      task(void)
      {
        if (!m_active)
          return;

        try
        {
          for (unsigned i = 0; i < m_args.data_points; ++i)
            ping(i);

          if (m_args.save_in_837)
            handleSonarData();
          else
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