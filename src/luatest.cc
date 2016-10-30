
#include <cassert>
#include <functional>
#include <iostream>
#include <tuple>

#define SOL_LUA_VERSION 501
#include <sol.hpp>

struct pending_node {
  bool has_tag(std::string key, std::string value) { return true; }
};

int main() {
  sol::state lua;

  lua.script_file("../profile/profile.lua");

  lua.new_usertype<pending_node>("pending_node",  //
                                 "has_tag", &pending_node::has_tag);


  pending_node node;


  // sol::function yolo = lua["yolo"];

  sol::optional<sol::function> x = lua["process_node"];
  if(!x) {
    std::cout << "process_node missing" << std::endl;
  }





  sol::function process_node = lua["process_node"];
  std::tuple<bool> result = process_node(node);

  std::cout << std::get<0>(result) << std::endl;

  // int x = 0;
  // lua.set_function("beep", [&x]{ ++x; });
  // lua.script("beep()");
  // std::cout << x << std::endl;
  // assert(x == 1);
}
