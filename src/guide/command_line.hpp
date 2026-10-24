// guide/command_line.hpp
#pragma once
#include <string_view>

// `KeyGuide --explain vim|blender|aerospace`: generates every missing tip for one keymap with the
// saved provider and model, exports its study guide and exits. No window or permissions needed.
int RunExplainCommand(std::string_view domain);
