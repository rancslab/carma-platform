# Copyright (C) 2022 LEIDOS.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from ament_index_python import get_package_share_directory
from launch.actions import Shutdown
from launch import LaunchDescription
from launch_ros.actions import Node
from launch_ros.actions import ComposableNodeContainer
from launch_ros.descriptions import ComposableNode
from launch.substitutions import EnvironmentVariable
from launch.substitutions import ThisLaunchFileDir
from carma_ros2_utils.launch.get_log_level import GetLogLevel
from carma_ros2_utils.launch.get_current_namespace import GetCurrentNamespace
from launch.substitutions import LaunchConfiguration

import os

from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.actions import GroupAction
from launch_ros.actions import set_remap
from launch.actions import DeclareLaunchArgument
from launch_ros.actions import PushRosNamespace


# Launch file for launching the nodes in the CARMA guidance stack

def generate_launch_description():

    route_file_folder = LaunchConfiguration('route_file_folder')
    vehicle_calibration_dir = LaunchConfiguration('vehicle_calibration_dir')
    vehicle_characteristics_param_file = LaunchConfiguration('vehicle_characteristics_param_file')
    enable_guidance_plugin_validator = LaunchConfiguration('enable_guidance_plugin_validator')
    strategic_plugins_to_validate = LaunchConfiguration('strategic_plugins_to_validate')
    tactical_plugins_to_validate = LaunchConfiguration('tactical_plugins_to_validate')
    control_plugins_to_validate = LaunchConfiguration('control_plugins_to_validate')
    vehicle_config_dir = LaunchConfiguration('vehicle_config_dir')
    vehicle_config_param_file = LaunchConfiguration('vehicle_config_param_file')
    declare_vehicle_config_param_file_arg = DeclareLaunchArgument(
        name = 'vehicle_config_param_file',
        default_value = "/opt/carma/vehicle/config/VehicleConfigParams.yaml",
        description = "Path to file contain vehicle configuration parameters"
    )

    subsystem_controller_default_param_file = os.path.join(
        get_package_share_directory('subsystem_controllers'), 'config/guidance_controller_config.yaml')

    mobilitypath_visualizer_param_file = os.path.join(
        get_package_share_directory('mobilitypath_visualizer'), 'config/params.yaml')

    trajectory_executor_param_file = os.path.join(
        get_package_share_directory('trajectory_executor'), 'config/parameters.yaml')
    
    route_param_file = os.path.join(
        get_package_share_directory('route'), 'config/parameters.yaml')
    env_log_levels = EnvironmentVariable('CARMA_ROS_LOGGING_CONFIG', default_value='{ "default_level" : "WARN" }')

    subsystem_controller_param_file = LaunchConfiguration('subsystem_controller_param_file')
    declare_subsystem_controller_param_file_arg = DeclareLaunchArgument(
        name = 'subsystem_controller_param_file',
        default_value = subsystem_controller_default_param_file,
        description = "Path to file containing override parameters for the subsystem controller"
    )

    # Below nodes are separated to individual container such that the nodes with reentrant services are within their separate container.
    # When all nodes are within single container, it is prone to fail throwing runtime_error, and it is currently hypothesized to be
    # because of this issue: https://github.com/ros2/rclcpp/issues/1212, where fix in the rclcpp library, so not able to be integrated at this moment:
    # https://github.com/ros2/rclcpp/pull/1241. This issue was first discovered in this carma issue: https://github.com/usdot-fhwa-stol/carma-platform/issues/1961  

    # Nodes
        # Nodes
    carma_guidance_container = ComposableNodeContainer(
        package='carma_ros2_utils',
        name='carma_guidance_container',
        executable='carma_component_container_mt',
        namespace=GetCurrentNamespace(),
        composable_node_descriptions=[

            ComposableNode(
                package='mobilitypath_visualizer',
                plugin='mobilitypath_visualizer::MobilityPathVisualizer',
                name='mobilitypath_visualizer_node',
                extra_arguments=[
                    {'use_intra_process_comms': True}, 
                    {'--log-level' : GetLogLevel('mobilitypath_visualizer', env_log_levels) }
                ],
                remappings = [
                    ("mobility_path_msg", [ EnvironmentVariable('CARMA_MSG_NS', default_value=''), "/mobility_path_msg" ] ),
                    ("incoming_mobility_path", [ EnvironmentVariable('CARMA_MSG_NS', default_value=''), "/incoming_mobility_path" ] ),
                    ("georeference", [ EnvironmentVariable('CARMA_LOCZ_NS', default_value=''), "/map_param_loader/georeference"])
                ],
                parameters=[
                    vehicle_characteristics_param_file,
                    mobilitypath_visualizer_param_file,
                    vehicle_config_param_file
                ]
            ),
            ComposableNode(
                package='trajectory_executor',
                plugin='trajectory_executor::TrajectoryExecutor',
                name='trajectory_executor_node',
                extra_arguments=[
                    {'use_intra_process_comms': True}, 
                    {'--log-level' : GetLogLevel('trajectory_executor', env_log_levels) }
                ],
                remappings = [
                    ("trajectory", "plan_trajectory"),
                ],
                parameters=[
                    trajectory_executor_param_file,
                    vehicle_config_param_file
                ]
            ),
            ComposableNode(
                package='route',
                plugin='route::Route',
                name='route_node',
                extra_arguments=[
                    {'use_intra_process_comms': True}, 
                    {'--log-level' : GetLogLevel('route', env_log_levels) }
                ],
                remappings = [
                    ("current_velocity", [ EnvironmentVariable('CARMA_INTR_NS', default_value=''), "/vehicle/twist" ] ),
                    ("georeference", [ EnvironmentVariable('CARMA_LOCZ_NS', default_value=''), "/map_param_loader/georeference" ] ),
                    ("semantic_map", [ EnvironmentVariable('CARMA_ENV_NS', default_value=''), "/semantic_map" ] ),
                    ("map_update", [ EnvironmentVariable('CARMA_ENV_NS', default_value=''), "/map_update" ] ),
                    ("roadway_objects", [ EnvironmentVariable('CARMA_ENV_NS', default_value=''), "/roadway_objects" ] ),
                    ("incoming_spat", [ EnvironmentVariable('CARMA_MSG_NS', default_value=''), "/incoming_spat" ] )
                ],
                parameters=[
                    {'route_file_path': route_file_folder},
                    route_param_file,
                    vehicle_config_param_file
                ]
            ),
        ]
    )

    # Launch plugins
    plugins_group = GroupAction(
        actions=[
            PushRosNamespace("plugins"),
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource([ThisLaunchFileDir(), '/plugins.launch.py']),
                launch_arguments={
                    'route_file_folder' : route_file_folder,
                    'vehicle_calibration_dir' : vehicle_calibration_dir,
                    'vehicle_characteristics_param_file' : vehicle_characteristics_param_file, 
                    'vehicle_config_param_file' : vehicle_config_param_file,
                    'enable_guidance_plugin_validator' : enable_guidance_plugin_validator,
                    'strategic_plugins_to_validate' : strategic_plugins_to_validate,
                    'tactical_plugins_to_validate' : tactical_plugins_to_validate,
                    'control_plugins_to_validate' : control_plugins_to_validate,
                    'subsystem_controller_param_file' : [vehicle_config_dir, '/SubsystemControllerParams.yaml'],
                }.items()
            ),
        ]
    )
      
    # subsystem_controller which orchestrates the lifecycle of this subsystem's components
    subsystem_controller = Node(
        package='subsystem_controllers',
        name='guidance_controller',
        executable='guidance_controller',
        parameters=[ subsystem_controller_default_param_file, subsystem_controller_param_file ],
        on_exit= Shutdown(), # Mark the subsystem controller as required
        arguments=['--ros-args', '--log-level', GetLogLevel('subsystem_controllers', env_log_levels)]
    )

    return LaunchDescription([  
        declare_vehicle_config_param_file_arg,
        declare_subsystem_controller_param_file_arg,  
        carma_guidance_container,   
        plugins_group,
        subsystem_controller
    ]) 
