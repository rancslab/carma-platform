// Copyright 2023 Leidos
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gtest/gtest.h>

#include <carma_cooperative_perception/j2735_types.hpp>
#include <carma_cooperative_perception/msg_conversion.hpp>
#include <carma_perception_msgs/msg/external_object.hpp>
#include <carma_perception_msgs/msg/external_object_list.hpp>

#include <numeric>

TEST(ToTimeMsg, HasSeconds)
{
  carma_cooperative_perception::DDateTime d_date_time;
  d_date_time.second = units::time::second_t{42.13};

  builtin_interfaces::msg::Time expected_msg;
  expected_msg.sec = 42.0;
  expected_msg.nanosec = 130'000'000;

  const auto actual_msg{carma_cooperative_perception::to_time_msg(d_date_time)};

  EXPECT_DOUBLE_EQ(actual_msg.sec, expected_msg.sec);
  EXPECT_DOUBLE_EQ(actual_msg.nanosec, expected_msg.nanosec);
}

TEST(ToTimeMsg, NulloptSeconds)
{
  const carma_cooperative_perception::DDateTime d_date_time;

  builtin_interfaces::msg::Time expected_msg;
  expected_msg.sec = 0.0;
  expected_msg.nanosec = 0;

  const auto actual_msg{carma_cooperative_perception::to_time_msg(d_date_time)};

  EXPECT_DOUBLE_EQ(actual_msg.sec, expected_msg.sec);
  EXPECT_DOUBLE_EQ(actual_msg.nanosec, expected_msg.nanosec);
}

TEST(ToDetectionMsg, Simple)
{
  carma_v2x_msgs::msg::SensorDataSharingMessage sdsm_msg;
  sdsm_msg.sdsm_time_stamp.second.millisecond = 1000;
  sdsm_msg.sdsm_time_stamp.presence_vector |= sdsm_msg.sdsm_time_stamp.SECOND;
  sdsm_msg.ref_pos.longitude = -90.703125;  // degrees
  sdsm_msg.ref_pos.latitude = 32.801128;    // degrees
  sdsm_msg.ref_pos.elevation_exists = true;
  sdsm_msg.ref_pos.elevation = 300.0;  // m

  carma_v2x_msgs::msg::DetectedObjectData object_data;
  object_data.detected_object_common_data.detected_id.object_id = 0xBEEF;
  object_data.detected_object_common_data.measurement_time.measurement_time_offset = -0.1;  // s

  object_data.detected_object_common_data.heading.heading = 34;  // true heading; degrees
  object_data.detected_object_common_data.obj_type.object_type =
    object_data.detected_object_common_data.obj_type.VEHICLE;

  object_data.detected_object_common_data.pos.offset_x.object_distance = 100.0;  // m
  object_data.detected_object_common_data.pos.offset_y.object_distance = 100.0;  // m

  object_data.detected_object_common_data.pos.presence_vector |=
    object_data.detected_object_common_data.pos.HAS_OFFSET_Z;
  object_data.detected_object_common_data.pos.offset_z.object_distance = 100.0;  // m

  object_data.detected_object_common_data.speed.speed = 10;    // m/s
  object_data.detected_object_common_data.speed_z.speed = 20;  // m/s

  object_data.detected_object_common_data.accel_4_way.longitudinal = 0.5;  // m/s^2
  object_data.detected_object_common_data.accel_4_way.lateral = 1.00;      // m/s^2
  object_data.detected_object_common_data.accel_4_way.vert = 23.536;       // m/s^2
  object_data.detected_object_common_data.accel_4_way.yaw_rate = 5.0;      // degrees/s

  sdsm_msg.objects.detected_object_data.push_back(object_data);

  const auto detection_list{carma_cooperative_perception::to_detection_list_msg(sdsm_msg)};
  ASSERT_EQ(std::size(detection_list.detections), 1U);

  const auto detection{detection_list.detections.at(0)};

  EXPECT_EQ(detection.header.stamp.sec, 0);
  EXPECT_NEAR(detection.header.stamp.nanosec, 900'000'000U, 2);  // +/- 2 ns is probably good enough
  EXPECT_EQ(detection.header.frame_id, "15N");

  EXPECT_NEAR(detection.pose.pose.position.x, 715068.54 + 100.0, 1e-2);   // m (ref pos + offset)
  EXPECT_NEAR(detection.pose.pose.position.y, 3631576.38 + 100.0, 1e-2);  // m (ref pos + offset)
  EXPECT_NEAR(detection.pose.pose.position.z, 300.0 + 100.0, 1e-3);       // m (ref pos + offset)
  EXPECT_DOUBLE_EQ(detection.pose.pose.orientation.x, 0.0);
  EXPECT_DOUBLE_EQ(detection.pose.pose.orientation.y, 0.0);
  EXPECT_NEAR(detection.pose.pose.orientation.z, 0.479035, 1e-5);
  EXPECT_NEAR(detection.pose.pose.orientation.w, 0.877796, 1e-5);

  EXPECT_DOUBLE_EQ(detection.twist.twist.linear.x, 10.0);
  EXPECT_DOUBLE_EQ(detection.twist.twist.linear.y, 0.0);
  EXPECT_DOUBLE_EQ(detection.twist.twist.linear.z, 20.0);
  EXPECT_DOUBLE_EQ(detection.twist.twist.angular.z, 5.0);

  EXPECT_DOUBLE_EQ(detection.accel.accel.linear.x, 0.5);
  EXPECT_DOUBLE_EQ(detection.accel.accel.linear.y, 1.0);
  EXPECT_NEAR(detection.accel.accel.linear.z, 2.4 * 9.80665, 1e-4);

  EXPECT_EQ(detection.id, std::to_string(0xBEEF));
  EXPECT_EQ(detection.motion_model, detection.MOTION_MODEL_CTRV);
}

TEST(CalcDetectionTimeStamp, Simple)
{
  carma_cooperative_perception::DDateTime d_date_time;
  d_date_time.second = units::time::second_t{5.0};

  carma_cooperative_perception::MeasurementTimeOffset offset{units::time::millisecond_t{2}};

  const auto stamp{carma_cooperative_perception::calc_detection_time_stamp(d_date_time, offset)};

  ASSERT_TRUE(stamp.second.has_value());
  EXPECT_DOUBLE_EQ(carma_cooperative_perception::remove_units(stamp.second.value()), 5.002);
}

TEST(ToPositionMsg, Simple)
{
  // Note: Google C++ style guide prohibits namespace using-directives
  using units::literals::operator""_m;

  constexpr carma_cooperative_perception::UtmZone zone{
    32, carma_cooperative_perception::Hemisphere::kNorth};
  constexpr carma_cooperative_perception::UtmCoordinate position_utm{zone, 12.0_m, 13.5_m, -0.5_m};
  const auto position_msg{carma_cooperative_perception::to_position_msg(position_utm)};

  EXPECT_DOUBLE_EQ(
    carma_cooperative_perception::remove_units(position_utm.easting), position_msg.x);
  EXPECT_DOUBLE_EQ(
    carma_cooperative_perception::remove_units(position_utm.northing), position_msg.y);
  EXPECT_DOUBLE_EQ(
    carma_cooperative_perception::remove_units(position_utm.elevation), position_msg.z);
}

// No ToDetectionListMsg test because a DetectionList.msg contains only a list of Detection.msg
// elements, the test for which is covered by ToDetectionMsg.
// TEST(ToDetectionListMsg, Simple) {}

TEST(ToDetectionMsg, FromExternalObject)
{
  carma_perception_msgs::msg::ExternalObject object;
  object.header.stamp.sec = 1;
  object.header.stamp.nanosec = 2;
  object.header.frame_id = "test_frame";
  object.bsm_id = {3, 4, 5, 6};
  object.id = 7;
  object.pose.pose.position.x = 8;
  object.pose.pose.position.y = 9;
  object.pose.pose.position.z = 10;
  object.pose.pose.orientation.x = 11;
  object.pose.pose.orientation.y = 12;
  object.pose.pose.orientation.z = 13;
  object.pose.pose.orientation.w = 14;
  object.velocity_inst.twist.linear.x = 15;
  object.velocity_inst.twist.linear.y = 16;
  object.velocity_inst.twist.linear.z = 17;
  object.velocity_inst.twist.angular.x = 18;
  object.velocity_inst.twist.angular.y = 19;
  object.velocity_inst.twist.angular.z = 20;
  object.object_type = object.SMALL_VEHICLE;

  object.presence_vector |= object.BSM_ID_PRESENCE_VECTOR | object.ID_PRESENCE_VECTOR |
                            object.POSE_PRESENCE_VECTOR | object.VELOCITY_INST_PRESENCE_VECTOR |
                            object.OBJECT_TYPE_PRESENCE_VECTOR;

  constexpr carma_cooperative_perception::MotionModelMapping motion_model_mapping{
    carma_cooperative_perception_interfaces::msg::Detection::MOTION_MODEL_CTRV,
    carma_cooperative_perception_interfaces::msg::Detection::MOTION_MODEL_CTRV,
    carma_cooperative_perception_interfaces::msg::Detection::MOTION_MODEL_CTRA,
    carma_cooperative_perception_interfaces::msg::Detection::MOTION_MODEL_CV,
    carma_cooperative_perception_interfaces::msg::Detection::MOTION_MODEL_CV};

  const auto detection{
    carma_cooperative_perception::to_detection_msg(object, motion_model_mapping)};

  EXPECT_EQ(detection.header, object.header);
  EXPECT_EQ(detection.id, "3456-7");
  EXPECT_EQ(detection.pose, object.pose);
  EXPECT_EQ(detection.twist, object.velocity_inst);
  EXPECT_EQ(detection.motion_model, detection.MOTION_MODEL_CTRV);
}

TEST(ToDetectionListMsg, FromExternalObjectList)
{
  carma_perception_msgs::msg::ExternalObjectList object_list;
  object_list.objects.emplace_back();
  object_list.objects.emplace_back();

  constexpr carma_cooperative_perception::MotionModelMapping motion_model_mapping{
    carma_cooperative_perception_interfaces::msg::Detection::MOTION_MODEL_CTRV,
    carma_cooperative_perception_interfaces::msg::Detection::MOTION_MODEL_CTRV,
    carma_cooperative_perception_interfaces::msg::Detection::MOTION_MODEL_CTRA,
    carma_cooperative_perception_interfaces::msg::Detection::MOTION_MODEL_CV,
    carma_cooperative_perception_interfaces::msg::Detection::MOTION_MODEL_CV};

  const auto detection_list{
    carma_cooperative_perception::to_detection_list_msg(object_list, motion_model_mapping)};

  EXPECT_EQ(std::size(detection_list.detections), 2U);
}

TEST(ToExternalObject, FromTrack)
{
  carma_cooperative_perception_interfaces::msg::Track track;
  track.header.stamp.sec = 1;
  track.header.stamp.nanosec = 2;
  track.header.frame_id = "test_frame";

  track.id = "1234";

  track.pose.pose.position.x = 1;
  track.pose.pose.position.y = 2;
  track.pose.pose.position.z = 3;

  track.pose.pose.orientation.x = 4;
  track.pose.pose.orientation.y = 5;
  track.pose.pose.orientation.z = 6;
  track.pose.pose.orientation.w = 7;

  std::iota(std::begin(track.pose.covariance), std::end(track.pose.covariance), 1U);

  track.twist.twist.linear.x = 1;
  track.twist.twist.linear.y = 2;
  track.twist.twist.linear.z = 3;

  track.twist.twist.angular.x = 4;
  track.twist.twist.angular.y = 5;
  track.twist.twist.angular.z = 6;

  std::iota(std::begin(track.twist.covariance), std::end(track.twist.covariance), 1U);

  const auto external_object{carma_cooperative_perception::to_external_object_msg(track)};

  EXPECT_TRUE(external_object.presence_vector & external_object.ID_PRESENCE_VECTOR);
  EXPECT_TRUE(external_object.presence_vector & external_object.POSE_PRESENCE_VECTOR);
  EXPECT_TRUE(external_object.presence_vector & external_object.VELOCITY_PRESENCE_VECTOR);

  EXPECT_EQ(external_object.id, 1234U);

  EXPECT_EQ(external_object.header, track.header);
  EXPECT_EQ(external_object.pose, track.pose);
  EXPECT_EQ(external_object.velocity, track.twist);
}

TEST(ToExternalObject, FromTrackNonNumericId)
{
  carma_cooperative_perception_interfaces::msg::Track track;
  track.header.stamp.sec = 1;
  track.header.stamp.nanosec = 2;
  track.header.frame_id = "test_frame";

  track.id = "abcd";

  track.pose.pose.position.x = 1;
  track.pose.pose.position.y = 2;
  track.pose.pose.position.z = 3;

  track.pose.pose.orientation.x = 4;
  track.pose.pose.orientation.y = 5;
  track.pose.pose.orientation.z = 6;
  track.pose.pose.orientation.w = 7;

  std::iota(std::begin(track.pose.covariance), std::end(track.pose.covariance), 1U);

  track.twist.twist.linear.x = 1;
  track.twist.twist.linear.y = 2;
  track.twist.twist.linear.z = 3;

  track.twist.twist.angular.x = 4;
  track.twist.twist.angular.y = 5;
  track.twist.twist.angular.z = 6;

  std::iota(std::begin(track.twist.covariance), std::end(track.twist.covariance), 1U);

  const auto external_object{carma_cooperative_perception::to_external_object_msg(track)};

  EXPECT_FALSE(external_object.presence_vector & external_object.ID_PRESENCE_VECTOR);
  EXPECT_TRUE(external_object.presence_vector & external_object.POSE_PRESENCE_VECTOR);
  EXPECT_TRUE(external_object.presence_vector & external_object.VELOCITY_PRESENCE_VECTOR);

  EXPECT_EQ(external_object.header, track.header);
  EXPECT_EQ(external_object.pose, track.pose);
  EXPECT_EQ(external_object.velocity, track.twist);
}

TEST(ToExternalObject, FromTrackNegativeId)
{
  carma_cooperative_perception_interfaces::msg::Track track;
  track.header.stamp.sec = 1;
  track.header.stamp.nanosec = 2;
  track.header.frame_id = "test_frame";

  track.id = "-1234";

  track.pose.pose.position.x = 1;
  track.pose.pose.position.y = 2;
  track.pose.pose.position.z = 3;

  track.pose.pose.orientation.x = 4;
  track.pose.pose.orientation.y = 5;
  track.pose.pose.orientation.z = 6;
  track.pose.pose.orientation.w = 7;

  std::iota(std::begin(track.pose.covariance), std::end(track.pose.covariance), 1U);

  track.twist.twist.linear.x = 1;
  track.twist.twist.linear.y = 2;
  track.twist.twist.linear.z = 3;

  track.twist.twist.angular.x = 4;
  track.twist.twist.angular.y = 5;
  track.twist.twist.angular.z = 6;

  std::iota(std::begin(track.twist.covariance), std::end(track.twist.covariance), 1U);

  const auto external_object{carma_cooperative_perception::to_external_object_msg(track)};

  EXPECT_FALSE(external_object.presence_vector & external_object.ID_PRESENCE_VECTOR);
  EXPECT_TRUE(external_object.presence_vector & external_object.POSE_PRESENCE_VECTOR);
  EXPECT_TRUE(external_object.presence_vector & external_object.VELOCITY_PRESENCE_VECTOR);

  EXPECT_EQ(external_object.header, track.header);
  EXPECT_EQ(external_object.pose, track.pose);
  EXPECT_EQ(external_object.velocity, track.twist);
}

TEST(ToExternalObject, FromTrackIdTooLarge)
{
  carma_cooperative_perception_interfaces::msg::Track track;
  track.header.stamp.sec = 1;
  track.header.stamp.nanosec = 2;
  track.header.frame_id = "test_frame";

  track.id = "5294967295";

  track.pose.pose.position.x = 1;
  track.pose.pose.position.y = 2;
  track.pose.pose.position.z = 3;

  track.pose.pose.orientation.x = 4;
  track.pose.pose.orientation.y = 5;
  track.pose.pose.orientation.z = 6;
  track.pose.pose.orientation.w = 7;

  std::iota(std::begin(track.pose.covariance), std::end(track.pose.covariance), 1U);

  track.twist.twist.linear.x = 1;
  track.twist.twist.linear.y = 2;
  track.twist.twist.linear.z = 3;

  track.twist.twist.angular.x = 4;
  track.twist.twist.angular.y = 5;
  track.twist.twist.angular.z = 6;

  std::iota(std::begin(track.twist.covariance), std::end(track.twist.covariance), 1U);

  const auto external_object{carma_cooperative_perception::to_external_object_msg(track)};

  EXPECT_FALSE(external_object.presence_vector & external_object.ID_PRESENCE_VECTOR);
  EXPECT_TRUE(external_object.presence_vector & external_object.POSE_PRESENCE_VECTOR);
  EXPECT_TRUE(external_object.presence_vector & external_object.VELOCITY_PRESENCE_VECTOR);

  EXPECT_EQ(external_object.header, track.header);
  EXPECT_EQ(external_object.pose, track.pose);
  EXPECT_EQ(external_object.velocity, track.twist);
}

TEST(ToExternalObjectList, FromTrackList)
{
  carma_cooperative_perception_interfaces::msg::TrackList track_list;
  track_list.tracks.push_back(carma_cooperative_perception_interfaces::msg::Track{});
  track_list.tracks.push_back(carma_cooperative_perception_interfaces::msg::Track{});
  track_list.tracks.push_back(carma_cooperative_perception_interfaces::msg::Track{});

  const auto external_object_list{
    carma_cooperative_perception::to_external_object_list_msg(track_list)};

  ASSERT_EQ(std::size(external_object_list.objects), 3U);
}