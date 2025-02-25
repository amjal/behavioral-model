/* Copyright 2013-present Barefoot Networks, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Antonin Bas (antonin@barefootnetworks.com)
 *
 */

/* Switch instance */

#include <bm/config.h>

#include <bm/SimpleSwitch.h>
#include <bm/bm_runtime/bm_runtime.h>
#include <bm/bm_sim/options_parse.h>
#include <bm/bm_sim/target_parser.h>

#include "simple_switch.h"

#include <csignal> //@amjall For handling SIGTERM from Mininet
#include <atomic> //@amjll for the signal flag

namespace {
SimpleSwitch *simple_switch;
}  // namespace


namespace sswitch_runtime {
shared_ptr<SimpleSwitchIf> get_handler(SimpleSwitch *sw);
}  // namespace sswitch_runtime

std::atomic<bool> sigterm_flag(false);
// @amjall Added to handle the SIGTERMs sent by Mininet 
// at the end of the simulations
void sigterm_handler(int signum){
	sigterm_flag.store(true);
}

int
main(int argc, char* argv[]) {
  signal(SIGTERM, sigterm_handler);
  bm::TargetParserBasicWithDynModules simple_switch_parser;
  simple_switch_parser.add_flag_option(
      "enable-swap",
      "Enable JSON swapping at runtime");
  simple_switch_parser.add_uint_option(
      "drop-port",
      "Choose drop port number (default is 511)");
  simple_switch_parser.add_uint_option(
      "priority-queues",
      "Number of priority queues (default is 1)");

  bm::OptionsParser parser;
  parser.parse(argc, argv, &simple_switch_parser);

  bool enable_swap_flag = false;
  if (simple_switch_parser.get_flag_option("enable-swap", &enable_swap_flag)
      != bm::TargetParserBasic::ReturnCode::SUCCESS) {
    std::exit(1);
  }

  uint32_t drop_port = 0xffffffff;
  {
    auto rc = simple_switch_parser.get_uint_option("drop-port", &drop_port);
    if (rc == bm::TargetParserBasic::ReturnCode::OPTION_NOT_PROVIDED)
      drop_port = SimpleSwitch::default_drop_port;
    else if (rc != bm::TargetParserBasic::ReturnCode::SUCCESS)
      std::exit(1);
  }

  uint32_t priority_queues = 0xffffffff;
  {
    auto rc = simple_switch_parser.get_uint_option(
        "priority-queues", &priority_queues);
    if (rc == bm::TargetParserBasic::ReturnCode::OPTION_NOT_PROVIDED)
      priority_queues = SimpleSwitch::default_nb_queues_per_port;
    else if (rc != bm::TargetParserBasic::ReturnCode::SUCCESS)
      std::exit(1);
  }

  simple_switch = new SimpleSwitch(enable_swap_flag, drop_port,
                                   priority_queues);

  int status = simple_switch->init_from_options_parser(parser);
  if (status != 0) std::exit(status);

  int thrift_port = simple_switch->get_runtime_port();
  bm_runtime::start_server(simple_switch, thrift_port);
  using ::sswitch_runtime::SimpleSwitchIf;
  using ::sswitch_runtime::SimpleSwitchProcessor;
  bm_runtime::add_service<SimpleSwitchIf, SimpleSwitchProcessor>(
      "simple_switch", sswitch_runtime::get_handler(simple_switch));
  simple_switch->start_and_return();
  while (!sigterm_flag.load())
	  std::this_thread::sleep_for(std::chrono::seconds(5));
  simple_switch->write_micro_logs();
  return 0;
}
