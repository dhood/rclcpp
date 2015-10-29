// Copyright 2015 Open Source Robotics Foundation, Inc.
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

#ifndef RCLCPP_RCLCPP_PARAMETER_SERVICE_HPP_
#define RCLCPP_RCLCPP_PARAMETER_SERVICE_HPP_

#include <string>

#include <rmw/rmw.h>

#include <rclcpp/executors.hpp>
#include <rclcpp/macros.hpp>
#include <rclcpp/node.hpp>
#include <rclcpp/parameter.hpp>

#include <rcl_interfaces/srv/describe_parameters.hpp>
#include <rcl_interfaces/srv/get_parameters.hpp>
#include <rcl_interfaces/srv/get_parameter_types.hpp>
#include <rcl_interfaces/srv/list_parameters.hpp>
#include <rcl_interfaces/srv/set_parameters.hpp>
#include <rcl_interfaces/srv/set_parameters_atomically.hpp>

namespace rclcpp
{

namespace parameter_service
{

class ParameterService
{

public:
  RCLCPP_SMART_PTR_DEFINITIONS(ParameterService);

  ParameterService(const rclcpp::node::Node::SharedPtr node);

private:
  const rclcpp::node::Node::SharedPtr node_;
  rclcpp::service::Service<rcl_interfaces::srv::GetParameters>::SharedPtr get_parameters_service_;
  rclcpp::service::Service<rcl_interfaces::srv::GetParameterTypes>::SharedPtr
    get_parameter_types_service_;
  rclcpp::service::Service<rcl_interfaces::srv::SetParameters>::SharedPtr set_parameters_service_;
  rclcpp::service::Service<rcl_interfaces::srv::SetParametersAtomically>::SharedPtr
    set_parameters_atomically_service_;
  rclcpp::service::Service<rcl_interfaces::srv::DescribeParameters>::SharedPtr
    describe_parameters_service_;
  rclcpp::service::Service<rcl_interfaces::srv::ListParameters>::SharedPtr list_parameters_service_;
};

} /* namespace parameter_service */

} /* namespace rclcpp */

#endif /* RCLCPP_RCLCPP_PARAMETER_SERVICE_HPP_ */
