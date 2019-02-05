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
/*******************************************************************************
 * libcadence Unit Test Suit
 * @file: test/test_9PMessageBuilder.cpp
 *
 *******************************************************************************/
#include "styxe/9p2000.hpp"  // Class being tested

#include <solace/exception.hpp>

#include <gtest/gtest.h>


using namespace Solace;
using namespace styxe;


class P9MessageBuilder : public ::testing::Test {
public:
    P9MessageBuilder() :
        _memManager(Protocol::MAX_MESSAGE_SIZE)
    {}


protected:

    void SetUp() override {
        _buffer = _memManager.allocate(Protocol::MAX_MESSAGE_SIZE);
        _buffer.viewRemaining().fill(0xFE);
    }

    MemoryManager   _memManager;
    ByteWriter     _buffer;
};


TEST_F(P9MessageBuilder, dirListingMessage) {
//    auto baseBuilder = ;
    auto responseWriter = Protocol::ResponseBuilder{_buffer, 1}
            .read(MemoryView{}); // Prime read request 0 size data!

    DirListingWriter writer{_buffer, 4096, 0};

    Protocol::Stat testStats[] = {
        {
            0,
            1,
            2,
            {2, 0, 64},
            01000644,
            0,
            0,
            4096,
            StringLiteral{"Root"},
            StringLiteral{"User"},
            StringLiteral{"Glanda"},
            StringLiteral{"User"}
        }
    };

    for (auto& stat : testStats) {
        stat.size = DirListingWriter::sizeStat(stat);
    }

    for (auto const& stat : testStats) {
        if (!writer.encode(stat))
            break;
    }

    auto& buf = responseWriter.build();
    ByteReader reader{buf.viewRemaining()};

    Protocol proc;
    auto maybeHeader = proc.parseMessageHeader(reader);
    ASSERT_TRUE(maybeHeader.isOk());

    auto maybeMessage = proc.parseResponse(maybeHeader.unwrap(), reader);
    ASSERT_TRUE(maybeMessage.isOk());

    auto& message = maybeMessage.unwrap();
    ASSERT_TRUE(std::holds_alternative<Protocol::Response::Read>(message));

    auto read = std::get<Protocol::Response::Read>(message);

    ASSERT_EQ(writer.bytesEncoded(), read.data.size());
}
