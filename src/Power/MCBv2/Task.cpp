//***************************************************************************
// Copyright (C) 2007-2013 Laboratório de Sistemas e Tecnologia Subaquática *
// Departamento de Engenharia Electrotécnica e de Computadores              *
// Rua Dr. Roberto Frias, 4200-465 Porto, Portugal                          *
//***************************************************************************
// Author: Ricardo Martins                                                  *
//***************************************************************************

// ISO C++ 98 headers.
#include <iostream>
#include <string>

// DUNE headers.
#include <DUNE/DUNE.hpp>

// Local headers.
#include "MCP23017.hpp"

#define MCP23017_ADDR 0x24
#define PARAMS_COUNT  6

// Parameters: index of ADC reference voltage * 10.
#define PARAMS_ADC_REF       0
// Parameters: index of battery current conversion factor * 100.
#define PARAMS_ADC_BAT_VOL   1
// Parameters: index of battery voltage conversion factor * 100.
#define PARAMS_ADC_BAT_AMP   2
// Parameters: index of system voltage conversion factor * 100.
#define PARAMS_ADC_SYS_VOL   3
// Parameters: index of system current conversion factor * 100.
#define PARAMS_ADC_SYS_AMP   4
// Parameters: charger current at which we consider the battery charged * 100.
#define PARAMS_CHARGED_AMP   5

namespace Power
{
  namespace MCBv2
  {
    using DUNE_NAMESPACES;

    //! Maximum number of ADC derived messages.
    static const unsigned c_adcs_count = 6;

    //! Commands to the device.
    enum Commands
    {
      CMD_STATE = 0x01,
      CMD_BLIGHT = 0x02,
      CMD_PARAMS = 0x03,
      CMD_SAVE = 0x04,
      CMD_HALT = 0x05
    };

    //! Power Bits.
    enum PowerBits
    {
      BIT_SW_SYS_ON = (1 << 7),
      BIT_SW_CHR_ON = (1 << 6)
    };

    //! List of Power Channels.
    enum PowerChannels
    {
      PCH_ATX = 0,
      PCH_12V_SPARE2 = 1,
      PCH_ETH_SWITCH = 2,
      PCH_12V_POE3 = 3,
      PCH_12V_POE2 = 4,
      PCH_12V_POE1 = 5,
      PCH_12V_AMODEM = 6,
      PCH_BAT_OUT = 7,
      PCH_CPU = 8,
      PCH_USB_HUB_P4 = 9,
      PCH_USB_HUB_P3 = 10,
      PCH_GPS = 11,
      PCH_HSDPA = 12,
      PCH_USB_HUB = 13,
      PCH_LCD_BLIGHT = 16
    };

    //! Power Channel data structure.
    struct PowerChannel
    {
      IMC::PowerChannelState state;
      double sched_on;
      double sched_off;

      PowerChannel(void)
      {
        resetSchedules();
      }

      void
      resetSchedules(void)
      {
        sched_on = -1;
        sched_off = -1;
      }
    };

    //! Task arguments.
    struct Arguments
    {
      //! Model.
      std::string model;
      //! I2C device.
      std::string i2c_dev;
      //! ADC Reference Voltage.
      double adc_ref;
      //! Charged current.
      double charged_amp;
      //! ADC Messages.
      std::string adc_messages[c_adcs_count];
      //! ADC entity labels.
      std::string adc_elabels[c_adcs_count];
      //! ADC conversion factors.
      std::vector<double> adc_factors[c_adcs_count];
    };

    struct Task: public Tasks::Task
    {
      //! Device I2C address.
      static const uint8_t c_addr = 0x10;
      //! Task arguments.
      Arguments m_args;
      //! Device protocol handler.
      Hardware::LUCL::Protocol m_proto;
      //! List of power channels.
      std::vector<PowerChannel*> m_pcs;
      //! True if power down is in progress.
      bool m_pwr_down;
      //! Power channels (1 bit per channel).
      uint16_t m_pwr_chns;
      //! GPIO controller.
      MCP23017* m_gpios;
      //! True if system was shutdown.
      bool m_halt;
      //! ADC messages.
      IMC::Message* m_adcs[c_adcs_count];

      Task(const std::string& name, Tasks::Context& ctx):
        Tasks::Task(name, ctx),
        m_pwr_down(false),
        m_gpios(NULL),
        m_halt(false)
      {
        std::memset(m_adcs, 0, sizeof(m_adcs));

        // Define configuration parameters.
        param("Model", m_args.model)
        .defaultValue("")
        .description("A320");

        param("I2C - Device", m_args.i2c_dev)
        .defaultValue("")
        .description("I2C device");

        param("ADC Reference Voltage", m_args.adc_ref)
        .defaultValue("1.1")
        .description("ADC reference voltage");

        param("Charged Current", m_args.charged_amp)
        .defaultValue("0.1")
        .units(Units::Ampere)
        .description("Charged current");

        for (unsigned i = 0; i < c_adcs_count; ++i)
        {
          std::string option = String::str("ADC Channel %u - Message", i);
          param(option, m_args.adc_messages[i]);

          option = String::str("ADC Channel %u - Entity Label", i);
          param(option, m_args.adc_elabels[i]);

          option = String::str("ADC Channel %u - Conversion", i);
          param(option, m_args.adc_factors[i])
          .size(2)
          .defaultValue("1.0, 0.0");
        }

        // Register consumers.
        bind<IMC::PowerChannelControl>(this);
      }

      ~Task(void)
      {
        for (unsigned i = 0; i < c_adcs_count; ++i)
        {
          if (m_adcs[i] != NULL)
            delete m_adcs[i];
        }

        for (unsigned i = 0; i < m_pcs.size(); ++i)
          delete m_pcs[i];

        if (m_gpios)
          delete m_gpios;
      }

      //! Update task parameters.
      void
      onUpdateParameters(void)
      {
        for (unsigned i = 0; i < c_adcs_count; ++i)
        {
          if (m_adcs[i] != NULL)
            delete m_adcs[i];

          m_adcs[i] = IMC::Factory::produce(m_args.adc_messages[i]);
        }
      }

      //! Reserve entities.
      void
      onEntityReservation(void)
      {
        for (unsigned i = 0; i < c_adcs_count; ++i)
        {
          unsigned eid = 0;

          try
          {
            eid = resolveEntity(m_args.adc_elabels[i]);
          }
          catch (Tasks::EntityDataBase::NonexistentLabel& e)
          {
            (void)e;
            eid = reserveEntity(m_args.adc_elabels[i]);
          }

          m_adcs[i]->setSourceEntity(eid);
        }
      }

      //! Acquire resources.
      void
      onResourceAcquisition(void)
      {
        m_proto.setI2C(m_args.i2c_dev, c_addr);
        m_proto.setName("MCB");
        m_proto.open();

        m_gpios = new MCP23017(m_args.i2c_dev, MCP23017_ADDR);
        m_pwr_chns = m_gpios->getGPIOs();

        // Create power channels.
        createPC(PCH_GPS, "GPS", 1);
        createPC(PCH_HSDPA, "HSDPA Modem", 1);
        createPC(PCH_12V_POE3, "+12VDC P.O.E. #3", 1);
        createPC(PCH_12V_POE2, "+12VDC P.O.E. #2", 1);
        createPC(PCH_12V_POE1, "+12VDC P.O.E. #1", 1);

        if (m_args.model == "A321")
        {
          createPC(PCH_12V_AMODEM, "Acoustic Modem", 1);
          createPC(PCH_BAT_OUT, "Battery Out", 1);
          createPC(PCH_USB_HUB_P4, "Ethernet Switch", 1);
        }
      }

      //! Initialize resources.
      void
      onResourceInitialization(void)
      {
        try
        {
          m_proto.requestName();
          waitForCommand(0);

          m_proto.requestVersion();
          waitForCommand(0);

          updateParams();
        }
        catch (std::exception& e)
        {
          err("%s", e.what());
        }
      }

      //! Write value to position in a given buffer.
      //! @param[in] index position index.
      //! @param[in] value value to be written.
      //! @param[out] bfr buffer.
      void
      packParam(unsigned index, double value, uint8_t* bfr)
      {
        uint16_t tmp = (uint16_t)(value);
        bfr[(index * 2)] = (uint8_t)(tmp);
        bfr[(index * 2) + 1] = (uint8_t)(tmp >> 8);
      }

      //! Update parameters.
      void
      updateParams(void)
      {
        uint8_t data[PARAMS_COUNT * 2] = {0};

        packParam(PARAMS_ADC_REF, m_args.adc_ref * 10, data);
        packParam(PARAMS_CHARGED_AMP, m_args.charged_amp * 100, data);
        packParam(PARAMS_ADC_BAT_VOL, m_args.adc_factors[0][0] * 100, data);
        packParam(PARAMS_ADC_BAT_AMP, m_args.adc_factors[1][0] * 100, data);
        packParam(PARAMS_ADC_SYS_VOL, m_args.adc_factors[2][0] * 100, data);
        packParam(PARAMS_ADC_SYS_AMP, m_args.adc_factors[3][0] * 100, data);

        m_proto.sendCommand(CMD_PARAMS, data, PARAMS_COUNT * 2);
        waitForCommand(CMD_PARAMS);
      }

      //! Create Power Channel.
      //! @param[in] id channel identifier.
      //! @param[in] label channel label.
      //! @param[in] state channel state.
      void
      createPC(uint8_t id, const std::string& label, uint8_t state)
      {
        PowerChannel* pc = new PowerChannel;
        pc->state.id = id;
        pc->state.label = label;
        pc->state.state = state;
        m_pcs.push_back(pc);
      }

      //! Dispatch power channels to bus.
      void
      dispatchPCs(void)
      {
        for (unsigned i = 0; i < m_pcs.size(); ++i)
          dispatch(m_pcs[i]->state);
      }

      void
      consume(const IMC::PowerChannelControl* msg)
      {
        if (m_halt)
          return;

        if (msg->id == PCH_CPU)
        {
          // We're dead after this but it might take a few moments, so
          // don't mess with the i2c bus.
          m_proto.sendCommand(CMD_HALT);
          m_halt = true;
          return;
        }

        if (msg->id == PCH_LCD_BLIGHT)
        {
          uint8_t state = (msg->op == IMC::PowerChannelControl::PCC_OP_TURN_ON) ? 1 : 0;
          m_proto.sendCommand(CMD_BLIGHT, &state, 1);
          return;
        }

        if (msg->op == IMC::PowerChannelControl::PCC_OP_TURN_OFF)
        {
          m_pwr_chns &= ~(1 << msg->id);
          if (!((m_pwr_chns & (1 << PCH_GPS)) || (m_pwr_chns & (1 << PCH_HSDPA))))
            m_pwr_chns &= ~(1 << PCH_USB_HUB);
        }
        else if (msg->op == IMC::PowerChannelControl::PCC_OP_TURN_ON)
        {
          m_pwr_chns |= (1 << msg->id);
          if (((m_pwr_chns & (1 << PCH_GPS)) || (m_pwr_chns & (1 << PCH_HSDPA))))
            m_pwr_chns |= (1 << PCH_USB_HUB);
        }
        else if (msg->op == IMC::PowerChannelControl::PCC_OP_TOGGLE)
        {
          m_pwr_chns ^= (1 << msg->id);
          if (((m_pwr_chns & (1 << PCH_GPS)) || (m_pwr_chns & (1 << PCH_HSDPA))))
            m_pwr_chns |= (1 << PCH_USB_HUB);
          else
            m_pwr_chns &= ~(1 << PCH_USB_HUB);
        }
        else if (msg->op == IMC::PowerChannelControl::PCC_OP_SAVE)
        {
          uint8_t data[2] = {(uint8_t)(m_pwr_chns >> 8), (uint8_t)m_pwr_chns};
          m_proto.sendCommand(CMD_SAVE, data, sizeof(data));
        }
        else if (msg->op == IMC::PowerChannelControl::PCC_OP_SCHED_ON)
          m_pcs[msg->id]->sched_on = Clock::get() + msg->sched_time;
        else if (msg->op == IMC::PowerChannelControl::PCC_OP_SCHED_OFF)
          m_pcs[msg->id]->sched_off = Clock::get() + msg->sched_time;
        else if (msg->op == IMC::PowerChannelControl::PCC_OP_SCHED_RESET)
        {
          m_pcs[msg->id]->sched_on = -1;
          m_pcs[msg->id]->sched_off = -1;
        }

        m_gpios->setGPIOs(m_pwr_chns);
      }

      void
      consume(const IMC::QueryPowerChannelState* msg)
      {
        (void)msg;
        dispatchPCs();
      }

      //! On Command call.
      //! @param[in] cmd command.
      //! @param[in] data buffer of data.
      //! @param[in] data_size size of data.
      void
      onCommand(uint8_t cmd, const uint8_t* data, int data_size)
      {
        if (cmd == CMD_STATE)
        {
          uint16_t unpack[] =
          {
            // Battery Voltage.
            (uint16_t)(data[0] | (data[4] & 0x3 << 0) << 8),
            // Battery Current.
            (uint16_t)(data[1] | (data[4] & 0x3 << 2) << 6),
            // System Voltage.
            (uint16_t)(data[2] | (data[4] & 0x3 << 4) << 4),
            // System Current.
            (uint16_t)(data[3] | (data[4] & 0x3 << 6) << 2),
            // +5V
            (uint16_t)(data[5] | (data[7] & 0x3 << 0) << 8),
            // +12V
            (uint16_t)(data[6] | (data[7] & 0x3 << 2) << 6),
          };

          sendMessages(unpack);

          // Check power off.
          if ((data[8] & BIT_SW_SYS_ON) == 0)
          {
            m_pwr_down = true;
            IMC::PowerOperation pop;
            pop.op = IMC::PowerOperation::POP_PWR_DOWN_IP;
            pop.time_remain = (float)(data[8] & 0x1F);
            dispatch(pop);
          }
          else if (m_pwr_down)
          {
            m_pwr_down = false;
            IMC::PowerOperation pop;
            pop.op = IMC::PowerOperation::POP_PWR_DOWN_ABORTED;
            dispatch(pop);
          }
        }
        (void)data_size;
      }

      //! Send Message.
      //! @param[in] unpack pointer to unpacked data buffer.
      void
      sendMessages(const uint16_t* unpack)
      {
        // Dispatch ADCs
        for (unsigned i = 0; i < c_adcs_count; ++i)
        {
          if (m_adcs[i] == NULL)
            continue;

          fp32_t tmp = m_args.adc_factors[i][0] * ((unpack[i] / 1024.0) * m_args.adc_ref) + m_args.adc_factors[i][1];
          m_adcs[i]->setValueFP(tmp);
          dispatch(m_adcs[i]);
        }
      }

      //! Wait for command.
      //! @param[in] code command code.
      //! @return true if succcessful, false otherwise.
      bool
      waitForCommand(uint8_t code)
      {
        LUCL::Command cmd;
        LUCL::CommandType type = m_proto.consumeData(cmd);

        switch (type)
        {
          case LUCL::CommandTypeNormal:
            onCommand(cmd.command.code, cmd.command.data, cmd.command.size);
            if (cmd.command.code == code)
              return true;
            break;

          case LUCL::CommandTypeVersion:
            onVersion(cmd.version.major, cmd.version.minor, cmd.version.patch);
            break;

          case LUCL::CommandTypeName:
            onName(cmd.name.data);
            break;

          case LUCL::CommandTypeInvalidVersion:
            err("%s", DTR(Status::getString(Status::CODE_INVALID_CHECKSUM)));
            break;

          case LUCL::CommandTypeError:
            err("%s: %s", DTR("device reported"), m_proto.getErrorString(cmd.error.code));
            break;

          case LUCL::CommandTypeInvalidChecksum:
            err("%s", DTR(Status::getString(Status::CODE_INVALID_CHECKSUM)));
            break;

          case LUCL::CommandTypeNone:
            break;

          default:
            break;
        }

        return false;
      }

      //! Flash firmware.
      //! @param[in] file file descriptor.
      void
      flashFirmware(const std::string& file)
      {
        setEntityState(IMC::EntityState::ESTA_BOOT, Status::CODE_INIT);
        inf(DTR("updating firmware"));
        LUCL::BootLoader lucb(m_proto, true);
        lucb.flash(file);
      }

      void
      onName(const std::string& name)
      {
        if (name == "LUCB")
        {
          std::string fmw = m_proto.searchNewFirmware(m_ctx.dir_fmw);
          if (!fmw.empty())
            flashFirmware(fmw);
        }
        setEntityState(IMC::EntityState::ESTA_NORMAL, Status::CODE_ACTIVE);
      }

      //! On version call.
      //! @param[in] major major version number.
      //! @param[in] minor minor version number.
      //! @param[in] patch patch version number.
      void
      onVersion(unsigned major, unsigned minor, unsigned patch)
      {
        inf(DTR("version: %u.%u.%u"), major, minor, patch);

        std::string fmw = m_proto.searchNewFirmware(m_ctx.dir_fmw, 2, minor, patch);
        if (!fmw.empty())
          flashFirmware(fmw);

        setEntityState(IMC::EntityState::ESTA_NORMAL, Status::CODE_ACTIVE);
      }

      //! Check schedules.
      void
      checkSchedules(void)
      {
        double now = Clock::get();
        IMC::PowerChannelControl pcc;

        for (unsigned i = 0; i < m_pcs.size(); ++i)
        {
          if (m_pcs[i]->sched_on != -1)
          {
            if (now >= m_pcs[i]->sched_on)
            {
              m_pcs[i]->sched_on = -1;
              pcc.id = m_pcs[i]->state.id;
              pcc.op = IMC::PowerChannelControl::PCC_OP_TURN_ON;
              dispatch(pcc);
            }
          }

          if (m_pcs[i]->sched_off != -1)
          {
            if (now >= m_pcs[i]->sched_off)
            {
              m_pcs[i]->sched_off = -1;
              pcc.id = m_pcs[i]->state.id;
              pcc.op = IMC::PowerChannelControl::PCC_OP_TURN_OFF;
              dispatch(pcc);
            }
          }
        }
      }

      void
      onMain(void)
      {
        while (!stopping())
        {
          waitForMessages(1.0);

          checkSchedules();

          try
          {
            if (!m_halt)
            {
              m_proto.sendCommand(CMD_STATE);
              waitForCommand(CMD_STATE);

              for (unsigned i = 0; i < m_pcs.size(); ++i)
                m_pcs[i]->state.state = (m_pwr_chns & (1 << m_pcs[i]->state.id)) != 0;

              dispatchPCs();
            }
          }
          catch (std::exception& e)
          {
            err("%s", e.what());
          }
        }
      }
    };
  }
}

DUNE_TASK