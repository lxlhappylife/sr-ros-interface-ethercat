/**
 * @file   sensor_updater.cpp
 * @author toni <toni@shadowrobot.com>
 * @date   20 Oct 2011
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
 * @brief This class is used to update the tactile sensor command.
 *
 *
 */


#include "sr_robot_lib/sensor_updater.hpp"
#include <boost/foreach.hpp>
#include <iostream>

namespace generic_updater
{
  SensorUpdater::SensorUpdater(std::vector<UpdateConfig> update_configs_vector, operation_mode::device_update_state::DeviceUpdateState update_state)
    : GenericUpdater(update_configs_vector, update_state)
  {
  }

  SensorUpdater::~SensorUpdater()
  {
  }

  operation_mode::device_update_state::DeviceUpdateState SensorUpdater::build_init_command(ETHERCAT_DATA_STRUCTURE_0200_PALM_EDC_COMMAND* command)
  {
    if(!mutex->try_lock())
      return update_state;

    if (update_state == operation_mode::device_update_state::INITIALIZATION)
    {
      if(initialization_configs_vector.size()>0)
      {
        ///////
        // First we ask for the next data we want to receive

          which_data_to_request ++;

          if( which_data_to_request >= initialization_configs_vector.size() )
            which_data_to_request = 0;

          //initialization data
          command->tactile_data_type = initialization_configs_vector[which_data_to_request].what_to_update;
          ROS_DEBUG_STREAM("Updating sensor initialization data type: "<<command->tactile_data_type << " | ["<<which_data_to_request<<"/"<<initialization_configs_vector.size()<<"] ");
      }
    }
    else
    {
      //For the last message sent when a change of update_state happens (after that we use build_command instead of build_init_command)
      //we use the first important message
      //This is to avoid sending a random command (initialization_configs_vector is empty at this time)
      command->tactile_data_type = static_cast<FROM_MOTOR_DATA_TYPE>(important_update_configs_vector[0].what_to_update);
      ROS_DEBUG_STREAM("Updating sensor important data type: "<<command->tactile_data_type << " | ["<<which_data_to_request<<"/"<<important_update_configs_vector.size()<<"] ");
    }
    mutex->unlock();

    return update_state;
  }

  operation_mode::device_update_state::DeviceUpdateState SensorUpdater::build_command(ETHERCAT_DATA_STRUCTURE_0200_PALM_EDC_COMMAND* command)
  {
    if(!mutex->try_lock())
      return update_state;

    ///////
    // First we ask for the next data we want to receive

      which_data_to_request ++;

      if( which_data_to_request >= important_update_configs_vector.size() )
        which_data_to_request = 0;

    if(!unimportant_data_queue.empty())
    {
      //an unimportant data is available
      command->tactile_data_type = unimportant_data_queue.front();
      unimportant_data_queue.pop();

      ROS_DEBUG_STREAM("Updating sensor unimportant data type: "<<command->tactile_data_type << " | queue size: "<<unimportant_data_queue.size());
    }
    else
    {
      //important data to update as often as possible
      command->tactile_data_type = important_update_configs_vector[which_data_to_request].what_to_update;
      ROS_DEBUG_STREAM("Updating sensor important data type: "<<command->tactile_data_type << " | ["<<which_data_to_request<<"/"<<important_update_configs_vector.size()<<"] ");
    }

    mutex->unlock();

    return update_state;
  }

  bool SensorUpdater::reset()
  {
    //We need to send the reset command twice in a row to make sure
    // the tactiles are reset.
    for( unsigned int i=0; i<2 ; ++i)
      unimportant_data_queue.push(TACTILE_SENSOR_TYPE_RESET_COMMAND);
  }
}




/* For the emacs weenies in the crowd.
Local Variables:
   c-basic-offset: 2
End:
*/
