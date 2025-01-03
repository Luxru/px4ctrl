#include "bridge.h"
#include <ros/init.h>
#include <ros/time.h>
#include <mavros_msgs/AttitudeTarget.h>
#include <mavros_msgs/CommandBool.h>
#include <mavros_msgs/CommandLong.h>
#include <mavros_msgs/SetMode.h>
#include <std_msgs/Bool.h>
#include <mavros_msgs/ExtendedState.h>
#include <mavros_msgs/State.h>
#include <nav_msgs/Odometry.h>
#include <sensor_msgs/Imu.h>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>

namespace px4ctrl {

    Px4CtrlRosBridge::Px4CtrlRosBridge(const ros::NodeHandle& nh, std::shared_ptr<Px4State> px4_state):nh_(nh),px4_state_(px4_state){
        px4_state_sub = nh_.subscribe<mavros_msgs::State>( "/mavros/state", 10, build_px4ros_cb(px4_state_->state));

        px4_extended_state_sub = nh_.subscribe<mavros_msgs::ExtendedState>( "/mavros/extended_state", 10, build_px4ros_cb(px4_state_->ext_state));

        vio_odom_sub = nh_.subscribe<nav_msgs::Odometry>( "odom", 100, build_px4ros_cb(px4_state_->odom), ros::VoidConstPtr(), ros::TransportHints().tcpNoDelay() );

        ctrl_cmd_sub = nh_.subscribe<px4ctrl_lux::Command>("cmd", 100, build_px4ros_cb(px4_state_->ctrl_command), ros::VoidConstPtr(), ros::TransportHints().tcpNoDelay() );

        px4_imu_sub = nh_.subscribe<sensor_msgs::Imu>( "/mavros/imu/data",  // Note: do NOT change it to /mavros/imu/data_raw !!!
                                                                    100,
                                                                    build_px4ros_cb(px4_state->imu),
                                                                    ros::VoidConstPtr(),
                                                                    ros::TransportHints().tcpNoDelay() );
        
        px4_bat_sub = nh_.subscribe< sensor_msgs::BatteryState>( "/mavros/battery", 100, build_px4ros_cb(px4_state_->battery), ros::VoidConstPtr(), ros::TransportHints().tcpNoDelay() );
        
        px4_cmd_pub           = nh_.advertise<mavros_msgs::AttitudeTarget >( "/mavros/setpoint_raw/attitude", 10 );
        px4_set_mode_client  = nh_.serviceClient< mavros_msgs::SetMode >( "/mavros/set_mode" );
        px4_arming_client = nh_.serviceClient< mavros_msgs::CommandBool >( "/mavros/cmd/arming" );
        px4_cmd_client = nh_.serviceClient<mavros_msgs::CommandLong >("/mavros/cmd/command");
        
        allow_cmdctrl_pub = nh_.advertise<std_msgs::Bool>("allow_cmd",10);

        spdlog::info("Init px4ros node");
        return;
    }

    void Px4CtrlRosBridge::spin_once(){
        ros::spinOnce();
        spdlog::debug("ros spin once");
    }

    bool Px4CtrlRosBridge::set_mode(const std::string &mode){
        if (px4_state_->state->value().first==nullptr){
            spdlog::error("px4 state is nullptr");
            return false;
        }

        if(px4_state_->state->value().first->mode == mode){
            spdlog::info("Already in {} mode", mode);
            return true;
        }

        mavros_msgs::SetMode set_mode;
        //call px4, set offboard
        set_mode.request.custom_mode = mode;
        if ( !( px4_set_mode_client.call( set_mode ) && set_mode.response.mode_sent)) {
            spdlog::error( "Enter {} rejected by PX4!", mode);
            return false;
        }else{
            spdlog::info( "Enter {} accepted by PX4!", mode);
            return true;
        }
    }

    bool Px4CtrlRosBridge::enter_offboard(){
        return set_mode(mavros_msgs::State::MODE_PX4_OFFBOARD);
    }

    bool Px4CtrlRosBridge::exit_offboard(){
        return set_mode(mavros_msgs::State::MODE_PX4_LAND);
    }

    bool Px4CtrlRosBridge::set_arm(const bool arm){
        if (px4_state_->state->value().first==nullptr){
            spdlog::error("px4 state is nullptr");
            return false;
        }
        mavros_msgs::CommandBool arm_cmd;
        arm_cmd.request.value = arm;
        if( px4_state_->state->value().first->mode != mavros_msgs::State::MODE_PX4_OFFBOARD){
            spdlog::error("Not in offboard mode, can't arm");
            return false;
        }
        if( arm==px4_state_->state->value().first->armed) {
            spdlog::info("Already in {} mode", arm?"ARM":"DISARM");
            return true;
        }
        if (!( px4_arming_client.call( arm_cmd ) && arm_cmd.response.success )){
            if ( arm )
                spdlog::error( "ARM rejected by PX4!" );
            else
                spdlog::error( "DISARM rejected by PX4!" );
            return false;
        }
        return true;
    }

    bool Px4CtrlRosBridge::force_disarm(){
        if (px4_state_->state->value().first==nullptr){
            spdlog::error("px4 state is nullptr");
            return false;
        }
        mavros_msgs::CommandLong disarm_cmd;
        disarm_cmd.request.command = 400;
        disarm_cmd.request.param1 = 0;
        disarm_cmd.request.param2 = 21196;
        disarm_cmd.request.param3 = 0;
        disarm_cmd.request.param4 = 0;
        disarm_cmd.request.param5 = 0;
        disarm_cmd.request.param6 = 0;
        disarm_cmd.request.param7 = 0;
        if( px4_state_->state->value().first->mode != mavros_msgs::State::MODE_PX4_OFFBOARD){
            spdlog::error("Not in offboard mode, can't disarm");
            return false;
        }
        if (!( px4_cmd_client.call( disarm_cmd ) && disarm_cmd.response.success )){
            spdlog::error( "DISARM rejected by PX4!" );
            return false;
        }
        return true;
    }

    bool Px4CtrlRosBridge::pub_bodyrates_target(const double thrust, const std::array<double, 3>& bodyrates){//输入的油门应该是映射后的px4油门
        mavros_msgs::AttitudeTarget msg;
        msg.header.stamp    = ros::Time::now();
        msg.header.frame_id = std::string( "FCU" );

        msg.type_mask = mavros_msgs::AttitudeTarget::IGNORE_ATTITUDE;

        msg.body_rate.x = bodyrates[0];
        msg.body_rate.y = bodyrates[1];
        msg.body_rate.z = bodyrates[2];

        msg.thrust = thrust;

        px4_cmd_pub.publish( msg );

        return true;
    }

    void Px4CtrlRosBridge::pub_allow_cmdctrl(bool allow){
        std_msgs::Bool msg;
        msg.data = allow;
        allow_cmdctrl_pub.publish(msg);
        return;
    }

    bool Px4CtrlRosBridge::pub_attitude_target(const double thrust, const std::array<double, 4>quat){//w,x,y,z
        mavros_msgs::AttitudeTarget msg;
        msg.header.stamp    = ros::Time::now();
        msg.header.frame_id = std::string( "FCU" );

        msg.type_mask = mavros_msgs::AttitudeTarget::IGNORE_ROLL_RATE | mavros_msgs::AttitudeTarget::IGNORE_PITCH_RATE | mavros_msgs::AttitudeTarget::IGNORE_YAW_RATE;

        msg.orientation.w = quat[0];
        msg.orientation.x = quat[1];
        msg.orientation.y = quat[2];
        msg.orientation.z = quat[3];
        msg.thrust = thrust;//9.81==>Hover thrust=9.81

        px4_cmd_pub.publish( msg );

        return true;
    }
}