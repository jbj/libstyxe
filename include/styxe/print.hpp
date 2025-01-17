/*
*  Copyright 2018 Ivan Ryabov
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
*/
#pragma once
#ifndef STYXE_PRINT_HPP
#define STYXE_PRINT_HPP

#include "9p2000.hpp"

#include <ostream>

namespace styxe {

    std::ostream& operator<< (std::ostream& ostr, MessageType t);

}  // end of namespace styxe
#endif  // STYXE_PRINT_HPP
