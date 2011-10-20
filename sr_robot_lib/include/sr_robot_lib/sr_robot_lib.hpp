/**
 * @file   sr_robot_lib.hpp
 * @author Ugo Cupcic <ugo@shadowrobot.com>
 * @date   Fri Jun  3 12:12:13 2011
 *
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
 * @brief This is a generic robot library for Shadow Robot's Hardware.
 *
 *
 */

#ifndef _SR_ROBOT_LIB_HPP_
#define _SR_ROBOT_LIB_HPP_

#include <boost/smart_ptr.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/thread.hpp>
#include <vector>
#include <deque>

//used to publish debug values
#include <std_msgs/Int16.h>

#include <sr_utilities/thread_safe_map.hpp>

#include <sr_hardware_interface/sr_actuator.hpp>

#include <sr_robot_msgs/ForceController.h>
#include <sr_robot_msgs/SetDebugData.h>

#include <sr_utilities/sr_math_utils.hpp>
#include <sr_utilities/calibration.hpp>

#include "sr_robot_lib/motor_updater.hpp"
#include "sr_robot_lib/generic_tactiles.hpp"

#include <sr_external_dependencies/types_for_external.h>
extern "C"
{
  #include <sr_external_dependencies/external/0220_palm_edc/0220_palm_edc_ethercat_protocol.h>
  #include <sr_external_dependencies/external/simplemotor-bootloader/bootloader.h>
}

namespace crc_unions
{
  typedef union
  {
    int16u word;
    int8u byte[2];
  } union16;
}

namespace shadow_joints
{
  struct PartialJointToSensor
  {
    int sensor_id;
    double coeff;
  };

  struct JointToSensor
  {
    std::vector<std::string> sensor_names;
    std::vector<PartialJointToSensor> joint_to_sensor_vector;
    bool calibrate_after_combining_sensors;
  };

  class Motor
  {
  public:
    Motor() :
      motor_id(0), msg_motor_id(0), actuator(NULL), motor_ok(false),
      bad_data(false)
    {};

    ~Motor(){};

    //the position of the motor in the motor array
    // coming from the hardware
    int motor_id;

    //the position of the motor in the message array
    int msg_motor_id;

    //actuator
    sr_actuator::SrActuator* actuator;

    /**
     * this boolean is set to true as long as we receive the
     * data from the motor.
     */
    bool motor_ok;
    /**
     * this boolean is set to true if the data coming from the motor
     * through the CAN bus are messed up.
     */
    bool bad_data;

    /**
     * A service used to set the force PID settings on the
     * motor.
     */
    ros::ServiceServer force_pid_service;

    /**
     * A service used to reset the
     * motors.
     */
    ros::ServiceServer reset_motor_service;
  };

  struct Joint
  {
    std::string joint_name;

    //the indexes of the joints in the joint array
    // coming from the hardware which are used to
    // compute the joint data.
    JointToSensor joint_to_sensor;

    //used to filter the position and the velocity
    sr_math_utils::filters::AlphaBetaFilter pos_filter;
    //used to filter the effort
    sr_math_utils::filters::AlphaBetaFilter effort_filter;

    bool has_motor;
    boost::shared_ptr<Motor> motor;
  };

  typedef threadsafe::Map<boost::shared_ptr<shadow_robot::JointCalibration> > CalibrationMap;
}

namespace shadow_robot
{
  class SrRobotLib
  {
  public:
    SrRobotLib(pr2_hardware_interface::HardwareInterface *hw);
    ~SrRobotLib() {};

    /**
     * This function is called each time a new etherCAT message
     * is received in the sr06.cpp driver. It updates the joints_vector,
     * updating the different values, computing the calibrated joint
     * positions, etc... It also updates the tactile sensors values.
     *
     * @param status_data the received etherCAT message
     */
    void update(ETHERCAT_DATA_STRUCTURE_0200_PALM_EDC_STATUS* status_data);

    /**
     * Builds a motor command: either send a torque demand or a configuration
     * demand if one is waiting.
     *
     * @param command The command we're building.
     */
    void build_motor_command(ETHERCAT_DATA_STRUCTURE_0200_PALM_EDC_COMMAND* command);

    /// The vector containing all the robot joints.
    boost::ptr_vector<shadow_joints::Joint> joints_vector;
    /// The map used to calibrate each joint.
    shadow_joints::CalibrationMap calibration_map;

    boost::shared_ptr<tactiles::GenericTactiles> tactiles;

    /**
     * Contains the idle time of the PIC communicating
     * via etherCAT with the host.
     */
    int main_pic_idle_time;

    /**
     * Contains the minimum idle time of the PIC communicating
     * via etherCAT with the host, this minimum is reset each
     * time a diagnostic is being published.
     */
    int main_pic_idle_time_min;

  protected:
    /**
     * Initializes the joints_vector.
     *
     * @param joint_names A vector containing all the joints.
     * @param motor_ids A vector containing the corresponding motor indexes (-1 if no motor is associated).
     * @param joint_to_sensors The mapping between the joints and the sensors (e.g. FFJ0 = FFJ1+FFJ2)
     * @param actuators The actuators.
     */
    virtual void initialize(std::vector<std::string> joint_names, std::vector<int> motor_ids,
                            std::vector<shadow_joints::JointToSensor> joint_to_sensors,
                            std::vector<sr_actuator::SrActuator*> actuators) = 0;

    /**
     * Compute the calibrated position for the given joint. This method is called
     * from the update method, each time a new message is received.
     *
     * @param joint_tmp The joint we want to calibrate.
     */
    void calibrate_joint(boost::ptr_vector<shadow_joints::Joint>::iterator joint_tmp);

    /**
     * Read additional data from the latest message and stores it into the
     * joints_vector.
     *
     * @param joint_tmp The joint we want to read teh data for.
     */
    void read_additional_data(boost::ptr_vector<shadow_joints::Joint>::iterator joint_tmp);

    /**
     * Transforms the incoming flag as a human
     * readable vector of strings.
     *
     * @param flag incoming flag.
     *
     * @return human readable flags.
     */
    std::vector<std::pair<std::string, bool> > humanize_flags(int flag);

    /**
     * Generates a force control config and adds it to the reconfig_queue with its
     * CRC. The config will be sent as soon as possible.
     *
     * @param motor_index The motor index.
     * @param max_pwm The max pwm the motor will apply
     * @param sg_left Strain gauge left
     * @param sg_right Strain gauge right
     * @param f The feedforward term (directly adds f*error to the output of the PID)
     * @param p The p value.
     * @param i the i value.
     * @param d the d value.
     * @param imax the imax value.
     * @param deadband the deadband on the force.
     * @param sign can be 0 or 1 depending on the way the motor is plugged in.
     */
    void generate_force_control_config(int motor_index, int max_pwm, int sg_left, int sg_right,
                                       int f, int p, int i, int d, int imax,
                                       int deadband, int sign);

    /**
     * The motor updater is used to create a correct command to send to the motor.
     * It's build_update_motor_command() is called each time the SR06::packCommand()
     * is called.
     */
    boost::shared_ptr<motor_updater::MotorUpdater> motor_updater_;

    /**
     * The ForceConfig type consists of an int representing the motor index for this config
     * followed by a vector of config: the index in the vector of config corresponds to the
     * type of the data, and the value at this index corresponds to the value we want to set.
     */
    typedef std::pair<int, std::vector<crc_unions::union16> > ForceConfig;
    /**
     * This queue contains the force PID config waiting to be pushed to the motor.
     */
    std::queue<ForceConfig, std::list<ForceConfig> > reconfig_queue;
    ///this index is used to iterate over the config we're sending.
    int config_index;

    ///contains a queue of motor indexes to reset
    std::queue<short, std::list<short> > reset_motors_queue;


    /// The current actuator.
    sr_actuator::SrActuator* actuator;
    /// The latest etherCAT message received.
    ETHERCAT_DATA_STRUCTURE_0200_PALM_EDC_STATUS* status_data;
    /// A temporary calibration for a given joint.
    boost::shared_ptr<shadow_robot::JointCalibration> calibration_tmp;

    ///The index of the motor in all the 20 motors
    int motor_index_full;
    ///The index of the motor in the current message (from 0 to 9)
    int index_motor_in_msg;

    int8u crc_byte;
    int16u crc_result;
    int8u crc_i;

    /// a ROS nodehandle to be able to advertise the Force PID service
    ros::NodeHandle nh_tilde;

#ifdef DEBUG_PUBLISHER
    ///These publishers are useful for debugging
    static const int nb_debug_publishers_const;
    std::vector<ros::Publisher> debug_publishers;
    /**
     * A vector containing pairs:
     *  - associate a motor index
     *  - to a MOTOR_DATA
     *
     * This vector has the same size as the debug_publishers vector.
     */
    std::vector<boost::shared_ptr<std::pair<int, int> > > debug_motor_indexes_and_data;
    static const int debug_mutex_lock_wait_time;
    boost::shared_mutex debug_mutex;
    ros::NodeHandle node_handle;
    std_msgs::Int16 msg_debug;
#endif

    ///We need to know if we're overflowing or not.
    int last_can_msgs_received;
    ///We need to know if we're overflowing or not.
    int last_can_msgs_transmitted;
  };//end class
}//end namespace

/* For the emacs weenies in the crowd.
Local Variables:
   c-basic-offset: 2
End:
*/

#endif

