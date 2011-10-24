/**
 * @file   shadow_PSTs.cpp
 * @author Toni Oliver <toni@shadowrobot.com>
 * @date   Th Oct 20 10:06:14 2011
 *
* Copyright 2011 Shadow Robot Company Ltd.
*
* This program is free software: you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the Free
* Software Foundation, either version 2 of the License, or (at your option)
* any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*
 * @brief This is a class for accessing the data from the
 *        PSTs tactiles.
 *
 *
 */

#include "sr_robot_lib/shadow_PSTs.hpp"
#include <sr_utilities/sr_math_utils.hpp>

namespace tactiles
{
  ShadowPSTs::ShadowPSTs(std::vector<generic_updater::UpdateConfig> update_configs_vector)
    : GenericTactiles(update_configs_vector)
  {
    // Tactile sensor real time publisher
    tactile_publisher = boost::shared_ptr<realtime_tools::RealtimePublisher<sr_robot_msgs::ShadowPST> >( new realtime_tools::RealtimePublisher<sr_robot_msgs::ShadowPST>(nodehandle_ , "tactile", 4));

    //initialize the vector of tactiles
    tactiles_vector = boost::shared_ptr< std::vector<PST3Data> >( new std::vector<PST3Data>(nb_tactiles) );
  }

  void ShadowPSTs::update(ETHERCAT_DATA_STRUCTURE_0200_PALM_EDC_STATUS* status_data)
  {
    int tactile_mask = static_cast<int16u>(status_data->tactile_data_valid);
    //TODO: use memcopy instead?
    for( unsigned int id_sensor = 0; id_sensor < nb_tactiles; ++id_sensor)
    {
      switch( static_cast<int32u>(status_data->tactile_data_type) )
      {
        //TACTILE DATA
      case TACTILE_SENSOR_TYPE_PST3_PRESSURE_TEMPERATURE:
        if( sr_math_utils::is_bit_mask_index_true(tactile_mask, id_sensor) )
        {
          tactiles_vector->at(id_sensor).pressure = static_cast<unsigned int>(static_cast<int16u>(status_data->tactile[id_sensor].word[0]) );
          tactiles_vector->at(id_sensor).temperature = static_cast<unsigned int>(static_cast<int16u>(status_data->tactile[id_sensor].word[1]) );
          tactiles_vector->at(id_sensor).debug_1 = static_cast<unsigned int>(static_cast<int16u>(status_data->tactile[id_sensor].word[2]) );
          tactiles_vector->at(id_sensor).debug_2 = static_cast<unsigned int>(static_cast<int16u>(status_data->tactile[id_sensor].word[3]) );
        }
        break;

      case TACTILE_SENSOR_TYPE_PST3_PRESSURE_RAW_ZERO_TRACKING:
        if( sr_math_utils::is_bit_mask_index_true(tactile_mask, id_sensor) )
        {
          tactiles_vector->at(id_sensor).pressure_raw = static_cast<unsigned int>(static_cast<int16u>(status_data->tactile[id_sensor].word[0]) );
          tactiles_vector->at(id_sensor).zero_tracking = static_cast<unsigned int>(static_cast<int16u>(status_data->tactile[id_sensor].word[1]) );
        }
        break;

      case TACTILE_SENSOR_TYPE_PST3_DAC_VALUE:
        if( sr_math_utils::is_bit_mask_index_true(tactile_mask, id_sensor) )
        {
          tactiles_vector->at(id_sensor).dac_value = static_cast<unsigned int>(static_cast<int16u>(status_data->tactile[id_sensor].word[0]) );
        }
        break;

        //COMMON DATA
      case TACTILE_SENSOR_TYPE_SAMPLE_FREQUENCY_HZ:
        if( sr_math_utils::is_bit_mask_index_true(tactile_mask, id_sensor) )
        {
          tactiles_vector->at(id_sensor).sample_frequency = static_cast<unsigned int>(static_cast<int16u>(status_data->tactile[id_sensor].word[0]) );
        }
        break;

      case TACTILE_SENSOR_TYPE_MANUFACTURER:
      {
        if( sr_math_utils::is_bit_mask_index_true(tactile_mask, id_sensor) )
        {
          std::string manufacturer = "";
          for (int i = 0; i < 16; ++i)
          {
            char tmp = static_cast<char>(status_data->tactile[id_sensor].string[i]);
            if( tmp != '0' )
              manufacturer += static_cast<char>(status_data->tactile[id_sensor].string[i]);
            else
              break;
          }
          tactiles_vector->at(id_sensor).manufacturer = manufacturer;
        }
      }
      break;

      case TACTILE_SENSOR_TYPE_SERIAL_NUMBER:
      {
        if( sr_math_utils::is_bit_mask_index_true(tactile_mask, id_sensor) )
        {
          std::string serial = "";
          for (int i = 0; i < 16; ++i)
          {
            char tmp = static_cast<char>(status_data->tactile[id_sensor].string[i]);
            if( tmp != 0 )
              serial += static_cast<char>(status_data->tactile[id_sensor].string[i]);
            else
              break;
          }
          tactiles_vector->at(id_sensor).serial_number = serial;
        }
      }
      break;

      case TACTILE_SENSOR_TYPE_SOFTWARE_VERSION:
        if( sr_math_utils::is_bit_mask_index_true(tactile_mask, id_sensor) )
        {
          tactiles_vector->at(id_sensor).software_version = static_cast<unsigned int>(static_cast<int16u>(status_data->tactile[id_sensor].word[0]) );
        }
        break;

      case TACTILE_SENSOR_TYPE_PCB_VERSION:
        if( sr_math_utils::is_bit_mask_index_true(tactile_mask, id_sensor) )
        {
          tactiles_vector->at(id_sensor).pcb_version = static_cast<unsigned int>(static_cast<int16u>(status_data->tactile[id_sensor].word[0]) );
        }
        break;

      default:
        break;

      } //end switch
    } //end for tactile
  }

  void ShadowPSTs::publish()
  {
    if(tactile_publisher->trylock())
    {
      //for the time being, we only have PSTs tactile sensors
      sr_robot_msgs::ShadowPST tactiles;
      tactiles.header.stamp = ros::Time::now();

      //tactiles.pressure.push_back(sr_hand_lib->tactile_data_valid);

      for(unsigned int id_tact = 0; id_tact < nb_tactiles; ++id_tact)
      {
        if( tactiles_vector->at(id_tact).tactile_data_valid )
        {
          tactiles.pressure.push_back( static_cast<int16u>(tactiles_vector->at(id_tact).pressure) );
          tactiles.temperature.push_back( static_cast<int16u>(tactiles_vector->at(id_tact).temperature) );
        }
        else
        {
          tactiles.pressure.push_back( -1 );
          tactiles.temperature.push_back( -1 );
        }
      }


      tactile_publisher->msg_ = tactiles;
      tactile_publisher->unlockAndPublish();
    }

  }//end publish

  void ShadowPSTs::add_diagnostics(std::vector<diagnostic_msgs::DiagnosticStatus> &vec,
                                   diagnostic_updater::DiagnosticStatusWrapper &d)
  {
    for(unsigned int id_tact = 0; id_tact < nb_tactiles; ++id_tact)
    {
      std::stringstream ss;

      ss << "Tactile " << id_tact + 1;

      d.name = ss.str().c_str();
      d.summary(d.OK, "OK");
      d.clear();

      d.addf("Sample Frequency", "%d", tactiles_vector->at(id_tact).sample_frequency);
      d.addf("Manufacturer", "%s", tactiles_vector->at(id_tact).manufacturer.c_str());
      d.addf("Serial Number", "%s", tactiles_vector->at(id_tact).serial_number.c_str());

      d.addf("Software Version", "%d", tactiles_vector->at(id_tact).software_version);
      d.addf("PCB Version", "%d", tactiles_vector->at(id_tact).pcb_version);

      d.addf("Pressure Raw", "%d", tactiles_vector->at(id_tact).pressure_raw);
      d.addf("Zero Tracking", "%d", tactiles_vector->at(id_tact).zero_tracking);
      d.addf("DAC Value", "%d", tactiles_vector->at(id_tact).dac_value);

      vec.push_back(d);
    }
  }
}

/* For the emacs weenies in the crowd.
   Local Variables:
   c-basic-offset: 2
   End:
*/


