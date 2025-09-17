#pragma once

#include "ucioption.h"
#include <iostream>
#include <sstream>
#include <string>

#define ENGINE_VERSION "0.4.1"

namespace uci {

void execute(const std::string& command);

}