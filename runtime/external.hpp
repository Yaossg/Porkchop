#pragma once

#include <functional>
#include <vector>

#include "../util.hpp"


namespace Porkchop {

struct VM;

using ExternalFunctionR = std::function<$union(VM*, std::vector<$union> const &)>;

namespace Externals {

$union print(VM* vm, std::vector<$union> const &args);
$union println(VM* vm, std::vector<$union> const &args);
$union readLine(VM* vm, std::vector<$union> const &args);
$union parseInt(VM* vm, std::vector<$union> const &args);
$union parseFloat(VM* vm, std::vector<$union> const &args);
$union exit(VM* vm, std::vector<$union> const &args);
$union millis(VM* vm, std::vector<$union> const &args);
$union nanos(VM* vm, std::vector<$union> const &args);
$union getargs(VM* vm, std::vector<$union> const &args);
$union output(VM* vm, std::vector<$union> const &args);
$union input(VM* vm, std::vector<$union> const &args);
$union flush(VM* vm, std::vector<$union> const &args);
$union eof(VM* vm, std::vector<$union> const &args);
$union typename_(VM* vm, std::vector<$union> const &args);
$union gc(VM* vm, std::vector<$union> const &args);
$union toBytes(VM* vm, std::vector<$union> const &args);
$union toChars(VM* vm, std::vector<$union> const &args);
$union fromBytes(VM* vm, std::vector<$union> const &args);
$union fromChars(VM* vm, std::vector<$union> const &args);

}

}