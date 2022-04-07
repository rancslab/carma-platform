/*
 * Copyright (C) 2019-2022 LEIDOS.
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
#include "motion_computation/motion_computation_worker.hpp"

namespace motion_computation{

    MotionComputationWorker::MotionComputationWorker(const PublishObjectCallback& obj_pub, rclcpp::node_interfaces::NodeLoggingInterface::SharedPtr logger) 
        : obj_pub_(obj_pub), logger_(logger) {};

    void MotionComputationWorker::predictionLogic(carma_perception_msgs::msg::ExternalObjectList::UniquePtr obj_list)
    {
        carma_perception_msgs::msg::ExternalObjectList output_list;
        carma_perception_msgs::msg::ExternalObjectList sensor_list;

        for (auto obj : obj_list->objects)
        {
            // Header contains the frame rest of the fields will use
            // obj.header = obj_list.objects[i].header;

            // Object id. Matching ids on a topic should refer to the same object within some time period, expanded
            // obj.id = obj_list.objects[i].id;

            // Update the object type and generate predictions using CV or CTRV vehicle models.
            // If the object is a bicycle or motor vehicle use CTRV otherwise use CV.

            bool use_ctrv_model;

            if (  obj.object_type == obj.UNKNOWN)
            {
                use_ctrv_model = true;
            }
            else if (obj.object_type == obj.MOTORCYCLE)
            {
                use_ctrv_model = true;
            }
            else if (obj.object_type == obj.SMALL_VEHICLE)
            {
                use_ctrv_model = true;
            }
            else if (obj.object_type == obj.LARGE_VEHICLE)
            {
                use_ctrv_model = true;
            }
            else if ( obj.object_type == obj.PEDESTRIAN)
            {
                use_ctrv_model = false;
            }
            else
            {
                obj.object_type = obj.UNKNOWN;
                use_ctrv_model = false;
            }//end if-else

            
            if (use_ctrv_model == true)
            {
                obj.predictions =
                    motion_predict::ctrv::predictPeriod(obj, prediction_time_step_, prediction_period_,
                                                        prediction_process_noise_max_, prediction_confidence_drop_rate_);
            }
            else
            {
                obj.predictions = motion_predict::cv::predictPeriod(
                    obj, prediction_time_step_, prediction_period_, cv_x_accel_noise_, cv_y_accel_noise_,
                    prediction_process_noise_max_, prediction_confidence_drop_rate_);
            }
            sensor_list.objects.emplace_back(obj);
        }//end for-loop


        // Synchronization priority
        // Sensor
        // BSM
        // PSM
        // MobilityPath
        // TODO add descriptive comments here
        const carma_perception_msgs::msg::ExternalObjectList& synchronization_base_objects;
        if (enable_sensor_processing) {

            synchronization_base_objects = sensor_list;

        } else if (enable_bsm_processing) {

            synchronization_base_objects = bsm_list;

        } else if (enable_psm_processing) {

            synchronization_base_objects = psm_list;

        } else if (enable_mobility_path_processing) {

            synchronization_base_objects = mobility_path_list_;

        } else {
            
            RCLCPP_WARN_STREAM(logger_->get_logger(), "Not configured to publish any data publishing empty object list. Operating like this is NOT advised.");
            // synchronization_base_objects.header.stamp = now();// TODO get access to clock
            obj_pub_(synchronization_base_objects);
            // TODO clear queues?
            return;
        }

        carma_perception_msgs::msg::ExternalObjectList current_output; // TODO we need to set the header for this
        current_output.objects.reserve(synchronization_base_objects.objects.size());

        if (enable_sensor_processing) {

            current_output = synchronizeAndAppend(sensor_list, current_output);

        }
        
        if (enable_bsm_processing) {

            current_output = synchronizeAndAppend(bsm_list, current_output);

        } 
        
        if (enable_psm_processing) {

            current_output = synchronizeAndAppend(psm_list, current_output);

        } 
        
        if (enable_mobility_path_processing) {
            
            current_output = synchronizeAndAppend(mobility_path_list_, current_output);

        }

        obj_pub_(output_list);

        // Clear mobility msg path queue since it is published
        mobility_path_list_.objects = {};
    }

    void MotionComputationWorker::georeferenceCallback(const std_msgs::msg::String::UniquePtr msg) 
    {
        // Build projector from proj string
        map_projector_ = std::make_shared<lanelet::projection::LocalFrameProjector>(msg->data.c_str()); 

        
        std::string axis = wgs84_utils::proj_tools::getAxisFromProjString(msg->data);  // Extract axis for orientation calc

        ROS_INFO_STREAM("Extracted Axis: " << axis);

        ned_in_map_rotation_ = wgs84_utils::proj_tools::getRotationOfNEDFromProjAxis(axis);  // Extract map rotation from axis

        ROS_DEBUG_STREAM("Extracted NED in Map Rotation (x,y,z,w) : ( "
                        << ned_in_map_rotation_.get().x() << ", " << ned_in_map_rotation_.get().y() << ", "
                        << ned_in_map_rotation_.get().z() << ", " << ned_in_map_rotation_.get().w());
    }
    
    void MotionComputationWorker::setPredictionTimeStep(double time_step)
    {
        prediction_time_step_ = time_step;
    }

    void MotionComputationWorker::setMobilityPathPredictionTimeStep(double time_step)
    {
        mobility_path_prediction_time_step_ = time_step;
    }

    void MotionComputationWorker::setPredictionPeriod(double period)
    {
        prediction_period_ = period;
    }

    void MotionComputationWorker::setXAccelerationNoise(double noise)
    {
        cv_x_accel_noise_ = noise;
    }

    void MotionComputationWorker::setYAccelerationNoise(double noise)
    {
        cv_y_accel_noise_ = noise;
    }

    void MotionComputationWorker::setProcessNoiseMax(double noise_max)
    {
        prediction_process_noise_max_ = noise_max;
    }

    void MotionComputationWorker::setConfidenceDropRate(double drop_rate)
    {
        prediction_confidence_drop_rate_ = drop_rate;
    }

    void MotionComputationWorker::setExternalObjectPredictionMode(int external_object_prediction_mode)
    {
        external_object_prediction_mode_ = static_cast<MotionComputationMode>(external_object_prediction_mode);
    }

    void MotionComputationWorker::mobilityPathCallback(const carma_v2x_msgs::msg::MobilityPath::UniquePtr msg)
    {
        if (!map_projector_) {
            RCLCPP_DEBUG_STREAM(logger_->get_logger(), "Map projection not available yet so ignoring MobilityPath messages");
            return;
        }

        if (!enable_mobility_path_processing) {
            RCLCPP_DEBUG_STREAM(logger_->get_logger(), "enable_mobility_path_processing is false so ignoring MobilityPath messages");
            return;
        }

        mobility_path_list_.objects.push_back(mobilityPathToExternalObject(msg));
    }

    void MotionComputationWorker::psmCallback(const carma_v2x_msgs::msg::PSM::UniquePtr msg)
    {
        if (!map_projector_) {
            RCLCPP_DEBUG_STREAM(logger_->get_logger(), "Map projection not available yet so ignoring PSM messages");
            return;
        }

        if (!enable_psm_processing) {
            RCLCPP_DEBUG_STREAM(logger_->get_logger(), "enable_psm_processing is false so ignoring PSM messages");
            return;
        }

        psm_list_.objects.push_back(mobilityPathToExternalObject(msg));
    }

    void MotionComputationWorker::bsmCallback(const carma_v2x_msgs::msg::BSM::UniquePtr msg)
    {
        if (!map_projector_) {
            RCLCPP_DEBUG_STREAM(logger_->get_logger(), "Map projection not available yet so ignoring PSM messages");
            return;
        }

        if (!enable_bsm_processing) {
            RCLCPP_DEBUG_STREAM(logger_->get_logger(), "enable_bsm_processing is false so ignoring BSM messages");
            return;
        }

        bsm_list_.objects.push_back(mobilityPathToExternalObject(msg));
    }

    carma_perception_msgs::msg::ExternalObject MotionComputationWorker::bsmToExternalObject(const carma_v2x_msgs::msg::BSM::UniquePtr& msg) const
    {
        carma_perception_msgs::msg::ExternalObject output;

        // Generate a unique object id from the bsm id
        output.id = 0
        for (int i = msg->core_data.id.size() - 1; i >= 0; i--) { // using signed iterator to handle empty case
            output.id |= msg->core_data.id[i] << (8*i);
        }

        output.bsm_id = msg->core_data.id;

        return output;
    }
    



    

    

    carma_perception_msgs::msg::ExternalObjectList MotionComputationWorker::synchronizeAndAppend(const carma_perception_msgs::msg::ExternalObjectList& sensor_list, carma_perception_msgs::msg::ExternalObjectList mobility_path_list) const
    {
        carma_perception_msgs::msg::ExternalObjectList output_list;
        // Compare time_stamps of first elements of each list as they are guaranteed to be the earliest of the respective lists
        
        for (auto &path: mobility_path_list.objects)
        {
            // interpolate and match timesteps
            path = matchAndInterpolateTimeStamp(path, rclcpp::Time(sensor_list.header.stamp));
        }
        
        output_list.objects.insert(output_list.objects.begin(),sensor_list.objects.begin(),sensor_list.objects.end());
        output_list.objects.insert(output_list.objects.end(),mobility_path_list.objects.begin(),mobility_path_list.objects.end());
        return output_list;
    }

    carma_perception_msgs::msg::ExternalObject MotionComputationWorker::matchAndInterpolateTimeStamp(carma_perception_msgs::msg::ExternalObject path, const rclcpp::Time& time_to_match) const
    {
        carma_perception_msgs::msg::ExternalObject output = path;
        // empty predictions
        output.predictions = {};

        // add the first point to start of the predictions to easily loop over
        carma_perception_msgs::msg::PredictedState prev_state;
        prev_state.header.stamp = output.header.stamp;
        prev_state.predicted_position.orientation = output.pose.pose.orientation ;         
        prev_state.predicted_velocity = output.velocity.twist ;    
        prev_state.predicted_position.position.x = output.pose.pose.position.x ;        
        prev_state.predicted_position.position.y = output.pose.pose.position.y ;        
        prev_state.predicted_position.position.z = output.pose.pose.position.z ;  
        path.predictions.insert(path.predictions.begin(), prev_state);

        rclcpp::Time curr_time_to_match = time_to_match;
        // because of this logic, we would not encounter mobility path
        // that starts later than the time we are trying to match (which is starting time of sensed objects)
        bool is_first_point = true;
        carma_perception_msgs::msg::PredictedState new_state;
        for (auto const& curr_state : path.predictions)
        { 
            if (curr_time_to_match > curr_state.header.stamp )
            {
                prev_state = curr_state;
                continue;
            }

            // reaching here means curr_state starts later than the time we are trying to match
            rclcpp::Duration delta_t = rclcpp::Time(curr_state.header.stamp) - curr_time_to_match;
            double ratio = delta_t.seconds() / mobility_path_prediction_time_step_;
            double delta_x = curr_state.predicted_position.position.x - prev_state.predicted_position.position.x;
            double delta_y = curr_state.predicted_position.position.y - prev_state.predicted_position.position.y;
            double delta_z = curr_state.predicted_position.position.z - prev_state.predicted_position.position.z;

            // copy old unchanged parts
            new_state.header.stamp = curr_time_to_match;
            new_state.predicted_velocity = prev_state.predicted_velocity;
            new_state.predicted_position.orientation = prev_state.predicted_position.orientation;

            // interpolate position
            // we are "stepping back in time" to match the position
            new_state.predicted_position.position.x = curr_state.predicted_position.position.x - delta_x * ratio; 
            new_state.predicted_position.position.y = curr_state.predicted_position.position.y - delta_y * ratio; 
            new_state.predicted_position.position.z = curr_state.predicted_position.position.z - delta_z * ratio; 

            if (is_first_point) // we store in the body if it is the first point, not predictions
            {
                output.header.stamp = curr_time_to_match;
                output.pose.pose.orientation = new_state.predicted_position.orientation;
                output.velocity.twist = new_state.predicted_velocity;
                output.pose.pose.position.x = new_state.predicted_position.position.x;
                output.pose.pose.position.y = new_state.predicted_position.position.y;
                output.pose.pose.position.z = new_state.predicted_position.position.z;
                is_first_point = false;
            }
            else
            {
                output.predictions.push_back(new_state);
            }

            prev_state = curr_state;
            curr_time_to_match += rclcpp::Duration(mobility_path_prediction_time_step_ * 1e9);
        }

    return output;
    }

} // namespace motion_computation