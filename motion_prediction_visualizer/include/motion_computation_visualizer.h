/*
 * Copyright (C) 2019-2020 LEIDOS.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

#ifndef MOTION_COMPUTATION_H
#define MOTION_COMPUTATION_H

#include <ros/ros.h>
// #include <carma_utils/CARMAUtils.h>
// #include <functional>
// #include "motion_computation_worker.h"
// #include <tf2_ros/transform_listener.h>
// #include <tf2/LinearMath/Transform.h>
// #include <tf2_geometry_msgs/tf2_geometry_msgs.h>
#include <cav_msgs/ExternalObject.h>
#include <cav_msgs/ExternalObjectList.h>
#include <visualization_msgs/Marker.h>
#include <cav_msgs/RoadwayObstacleList.h>


class MotionPredictionVisualizer
{

 private:
  
  //node handle
  // ros::CARMANodeHandle nh_;
  // ros::CARMANodeHandle pnh_ = {"~"};
   
  // //subscriber
  // ros::Subscriber motion_comp_sub_;
  // ros::Subscriber mobility_path_sub_;

  // //publisher
  // ros::Publisher carma_obj_pub_;
  
  // //MotionComputationWorker class object
  // MotionComputationWorker motion_worker_;
  
  //   /*!fn initialize()
  // \brief initialize this node before running
  // */
  // void initialize();

  // // tf buffer holds the tree of transforms
  // tf2_ros::Buffer tf_buffer_;
  // std::unique_ptr<tf2_ros::TransformListener> tf2_listener_;
  
  // /*!fn initialize()
  // \brief lookup ECEF to Map transform from tf2 tree
  // \return ECEF to Map transform
  // */
  // tf2::Transform lookupECEFtoMapTransform();

 public:
  
   /*! \fn MotionPredictionVisualizer()
    \brief MotionPredictionVisualizer constructor 
   */
  MotionPredictionVisualizer();

     /*! \fn publishObject()
    \brief Callback to publish ObjectList
   */
  // void publishObject(const cav_msgs::ExternalObjectList& obj_pred_msg) const;

  /*!fn run()
  \brief General starting point to run this node
  */
  // void run();

  ros::Publisher marker_pub;

};

#endif /* MOTION_COMPUTATION_H */
