/*********************************************************************
* Software License Agreement (BSD License)
*
*  Copyright (c) 2008, Willow Garage, Inc.
*  All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of Willow Garage, Inc. nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

/* Author: Ioan Sucan */

#include <moveit/kinematic_trajectory/kinematic_trajectory.h>
#include <moveit/kinematic_state/conversions.h>
#include <boost/math/constants/constants.hpp>
#include <numeric>

kinematic_trajectory::KinematicTrajectory::KinematicTrajectory(const kinematic_model::KinematicModelConstPtr &kmodel, const std::string &group) : 
  kmodel_(kmodel),
  group_(group.empty() ? NULL : kmodel->getJointModelGroup(group))
{
}

void kinematic_trajectory::KinematicTrajectory::setGroupName(const std::string &group_name)
{
  group_ = kmodel_->getJointModelGroup(group_name);
}

const std::string& kinematic_trajectory::KinematicTrajectory::getGroupName() const
{
  if (group_)
    return group_->getName();
  static const std::string empty;
  return empty;
}

double kinematic_trajectory::KinematicTrajectory::getAverageSegmentDuration(void) const
{
  if (duration_from_previous_.empty())
    return 0.0;
  else
    return std::accumulate(duration_from_previous_.begin(), duration_from_previous_.end(), 0.0) / (double)duration_from_previous_.size();
}

void kinematic_trajectory::KinematicTrajectory::swap(kinematic_trajectory::KinematicTrajectory &other)
{
  kmodel_.swap(other.kmodel_);
  std::swap(group_, other.group_);
  waypoints_.swap(other.waypoints_);
  duration_from_previous_.swap(duration_from_previous_);
}

void kinematic_trajectory::KinematicTrajectory::swap(std::vector<kinematic_state::KinematicStatePtr> &other)
{
  waypoints_.swap(other);
  duration_from_previous_.clear();
}

void kinematic_trajectory::KinematicTrajectory::append(const KinematicTrajectory &source, double dt)
{
  waypoints_.insert(waypoints_.end(), source.waypoints_.begin(),source.waypoints_.end());
  std::size_t index = duration_from_previous_.size();
  duration_from_previous_.insert(duration_from_previous_.end(), source.duration_from_previous_.begin(),source.duration_from_previous_.end());
  if (duration_from_previous_.size() > index)
    duration_from_previous_[index] += dt;
}

void kinematic_trajectory::KinematicTrajectory::reverse(void)
{
  std::reverse(waypoints_.begin(), waypoints_.end());
  if (!duration_from_previous_.empty())
  {
    duration_from_previous_.push_back(duration_from_previous_.front());
    std::reverse(duration_from_previous_.begin(), duration_from_previous_.end());
    duration_from_previous_.pop_back();
  }
}

void kinematic_trajectory::KinematicTrajectory::unwind(const kinematic_state::KinematicState &state)
{
  if (waypoints_.empty())
    return;
  
  const std::vector<const kinematic_model::JointModel*> &cont_joints = group_ ? 
    group_->getContinuousJointModels() : kmodel_->getContinuousJointModels();
  
  for (std::size_t i = 0 ; i < cont_joints.size() ; ++i)
  {
    const kinematic_state::JointState *jstate = state.getJointState(cont_joints[i]);
    std::vector<double> reference_value = jstate->getVariableValues();
    jstate->getJointModel()->enforceBounds(reference_value);
    
    // unwrap continuous joints
    double running_offset = jstate->getVariableValues()[0] - reference_value[0];
    double last_value = waypoints_[0]->getJointState(cont_joints[i])->getVariableValues()[0];
    for (std::size_t j = 1 ; j < waypoints_.size() ; ++j)
    {
      kinematic_state::JointState *js = waypoints_[j]->getJointState(cont_joints[i]);
      std::vector<double> current_value = js->getVariableValues();
      if (last_value > current_value[0] + boost::math::constants::pi<double>())
        running_offset += 2.0 * boost::math::constants::pi<double>();
      else
        if (current_value[0] > last_value + boost::math::constants::pi<double>())
          running_offset -= 2.0 * boost::math::constants::pi<double>();
      last_value = current_value[0];
      if (running_offset > std::numeric_limits<double>::epsilon() || running_offset < -std::numeric_limits<double>::epsilon())
      {
        current_value[0] += running_offset;
        js->setVariableValues(current_value);
      }    
    }
  }
}

void kinematic_trajectory::KinematicTrajectory::clear()
{
  waypoints_.clear();
  duration_from_previous_.clear();
}

void kinematic_trajectory::KinematicTrajectory::getRobotTrajectoryMsg(moveit_msgs::RobotTrajectory &trajectory) const
{
  trajectory = moveit_msgs::RobotTrajectory();
  if (waypoints_.empty())
    return;
  const std::vector<const kinematic_model::JointModel*> &jnt = group_ ? group_->getJointModels() : kmodel_->getJointModels();
  
  std::vector<const kinematic_model::JointModel*> onedof;
  std::vector<const kinematic_model::JointModel*> mdof;
  trajectory.joint_trajectory.joint_names.clear();
  trajectory.multi_dof_joint_trajectory.joint_names.clear();
  
  for (std::size_t i = 0 ; i < jnt.size() ; ++i)
    if (jnt[i]->getVariableCount() == 1)
    {
      trajectory.joint_trajectory.joint_names.push_back(jnt[i]->getName());
      onedof.push_back(jnt[i]);
    }
    else
    {
      trajectory.multi_dof_joint_trajectory.joint_names.push_back(jnt[i]->getName());
      mdof.push_back(jnt[i]);
    }
  if (!onedof.empty())
  {  
    trajectory.joint_trajectory.header.frame_id = kmodel_->getModelFrame();
    trajectory.joint_trajectory.header.stamp = ros::Time::now();
    trajectory.joint_trajectory.points.resize(waypoints_.size());
  }
  
  if (!mdof.empty())
  {
    trajectory.multi_dof_joint_trajectory.header.frame_id = kmodel_->getModelFrame();
    trajectory.multi_dof_joint_trajectory.header.stamp = ros::Time::now();
    trajectory.multi_dof_joint_trajectory.points.resize(waypoints_.size());
  }
  
  static const ros::Duration zero_duration(0.0);
  double total_time = 0.0;
  for (std::size_t i = 0 ; i < waypoints_.size() ; ++i)
  {
    if (duration_from_previous_.size() > i)
      total_time += duration_from_previous_[i];
    
    if (!onedof.empty())
    {
      trajectory.joint_trajectory.points[i].positions.resize(onedof.size());
      for (std::size_t j = 0 ; j < onedof.size() ; ++j)
	trajectory.joint_trajectory.points[i].positions[j] = waypoints_[i]->getJointState(onedof[j]->getName())->getVariableValues()[0];
      if (duration_from_previous_.size() > i)
        trajectory.joint_trajectory.points[i].time_from_start = ros::Duration(total_time);
      else
        trajectory.joint_trajectory.points[i].time_from_start = zero_duration;
    }
    if (!mdof.empty())
    {
      trajectory.multi_dof_joint_trajectory.points[i].values.resize(mdof.size());
      for (std::size_t j = 0 ; j < mdof.size() ; ++j)
        trajectory.multi_dof_joint_trajectory.points[i].values[j].values = waypoints_[i]->getJointState(mdof[j]->getName())->getVariableValues();
      if (duration_from_previous_.size() > i)
        trajectory.multi_dof_joint_trajectory.points[i].time_from_start = ros::Duration(total_time);
      else
        trajectory.multi_dof_joint_trajectory.points[i].time_from_start = zero_duration;
    }
  }
}

void kinematic_trajectory::KinematicTrajectory::setRobotTrajectoryMsg(const kinematic_state::KinematicState &reference_state,
                                                                      const moveit_msgs::RobotTrajectory &trajectory)
{
  // make a copy just in case the next clear() removes the memory for the reference passed in
  kinematic_state::KinematicState copy = reference_state;
  clear();

  std::size_t state_count = std::max(trajectory.joint_trajectory.points.size(),
                                     trajectory.multi_dof_joint_trajectory.points.size());
  ros::Time last_time_stamp = trajectory.joint_trajectory.points.empty() ? trajectory.multi_dof_joint_trajectory.header.stamp : trajectory.joint_trajectory.header.stamp;
  ros::Time this_time_stamp = last_time_stamp;
  
  for (std::size_t i = 0 ; i < state_count ; ++i)
  {
    moveit_msgs::RobotState rs;
    if (trajectory.joint_trajectory.points.size() > i)
    {
      rs.joint_state.header = trajectory.joint_trajectory.header;
      rs.joint_state.header.stamp = rs.joint_state.header.stamp + trajectory.joint_trajectory.points[i].time_from_start;
      rs.joint_state.name = trajectory.joint_trajectory.joint_names;
      rs.joint_state.position = trajectory.joint_trajectory.points[i].positions;
      rs.joint_state.velocity = trajectory.joint_trajectory.points[i].velocities;
      this_time_stamp = rs.joint_state.header.stamp;
    }
    if (trajectory.multi_dof_joint_trajectory.points.size() > i)
    {
      rs.multi_dof_joint_state.joint_names = trajectory.multi_dof_joint_trajectory.joint_names;
      rs.multi_dof_joint_state.header.stamp = trajectory.joint_trajectory.header.stamp + trajectory.multi_dof_joint_trajectory.points[i].time_from_start;
      rs.multi_dof_joint_state.joint_values = trajectory.multi_dof_joint_trajectory.points[i].values;
      this_time_stamp = rs.multi_dof_joint_state.header.stamp;
    }
    
    kinematic_state::KinematicStatePtr st(new kinematic_state::KinematicState(copy));
    kinematic_state::robotStateToKinematicState(rs, *st);
    addWayPoint(st, (this_time_stamp - last_time_stamp).toSec());
    last_time_stamp = this_time_stamp;
  }
}

void kinematic_trajectory::KinematicTrajectory::setRobotTrajectoryMsg(const kinematic_state::KinematicState &reference_state,
                                                                      const moveit_msgs::RobotState &state, const moveit_msgs::RobotTrajectory &trajectory)
{
  kinematic_state::KinematicState st(reference_state);
  kinematic_state::robotStateToKinematicState(state, st);
  setRobotTrajectoryMsg(st, trajectory);
}
