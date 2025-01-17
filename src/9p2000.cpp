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

#include "styxe/9p2000.hpp"
#include "styxe/decoder.hpp"
#include "styxe/version.hpp"

#include <solace/assert.hpp>
#include <algorithm>  // std::min


using namespace Solace;
using namespace styxe;


static const Version   kLibVersion{STYXE_VERSION_MAJOR, STYXE_VERSION_MINOR, STYXE_VERSION_BUILD};

const size_type         styxe::kMaxMesssageSize = 8*1024;      // 8k should be enough for everyone, am I right?

const StringLiteral     Parser::PROTOCOL_VERSION = "9P2000.e";  // By default we want to talk via 9P2000.e proc
const StringLiteral     Parser::UNKNOWN_PROTOCOL_VERSION = "unknown";
const Tag               Parser::NO_TAG = static_cast<Tag>(~0);
const Fid               Parser::NOFID = static_cast<Fid>(~0);


const byte OpenMode::READ;
const byte OpenMode::WRITE;
const byte OpenMode::RDWR;
const byte OpenMode::EXEC;
const byte OpenMode::TRUNC;
const byte OpenMode::CEXEC;
const byte OpenMode::RCLOSE;


AtomValue const
styxe::kProtocolErrorCatergory = atom("9p2000");


#define CANNE(id, msg) \
    Error(kProtocolErrorCatergory, static_cast<uint16>(id), msg)


static Error const kCannedErrors[] = {
    CANNE(CannedError::IllFormedHeader, "Ill-formed message header. Not enough data to read a header"),
    CANNE(CannedError::IllFormedHeader_FrameTooShort, "Ill-formed message: Declared frame size less than header"),
    CANNE(CannedError::IllFormedHeader_TooBig, "Ill-formed message: Declared frame size greater than negotiated one"),
    CANNE(CannedError::UnsupportedMessageType, "Ill-formed message: Unsupported message type"),

    CANNE(CannedError::NotEnoughData, "Ill-formed message: Declared frame size larger than message data received"),
    CANNE(CannedError::MoreThenExpectedData, "Ill-formed message: Declared frame size less than message data received"),
};


Error
styxe::getCannedError(CannedError errorId) noexcept {
    return kCannedErrors[static_cast<int>(errorId)];
}

Version const& styxe::getVersion() noexcept {
    return kLibVersion;
}

namespace  {  // Internal imlpementation details

struct OkRespose {
    Result<ResponseMessage, Error>
	operator() (Decoder&) { return {types::okTag, std::move(fcall)}; }

    ResponseMessage fcall;

    template<typename T>
    OkRespose(T&& f)
        : fcall{std::forward<T>(f)}
    {}
};

struct OkRequest {
    Result<RequestMessage, Error>
	operator() (Decoder&) { return {types::okTag, std::move(fcall)}; }

    RequestMessage fcall;

    template<typename T>
    OkRequest(T&& f)
        : fcall{std::forward<T>(f)}
    {}
};


template<typename T>
Result<ResponseMessage, Error>
parseNoDataResponse() {
	return Result<ResponseMessage, Error>{types::okTag, T{}};
}


Result<ResponseMessage, Error>
parseErrorResponse(ByteReader& data) {
    Response::Error fcall;

	Decoder decoder{data};
	auto result = (decoder >> fcall.ename);

	return result
			.then(OkRespose{std::move(fcall)});
}


Result<ResponseMessage, Error>
parseVersionResponse(ByteReader& data) {
    Response::Version fcall;

	Decoder decoder{data};

	Solace::Result<Decoder&, Error> result = (decoder >> fcall.msize);
	result = std::move(result) >> fcall.version;

	return result
			.then(OkRespose{std::move(fcall)});
}


Result<ResponseMessage, Error>
parseAuthResponse(ByteReader& data) {
    Response::Auth fcall;

	Decoder decoder{data};
	auto result = decoder >> fcall.qid;
	return result
			.then(OkRespose{std::move(fcall)});
}


Result<ResponseMessage, Error>
parseAttachResponse(ByteReader& data) {
    Response::Attach fcall;

	Decoder decoder{data};
	auto result = decoder >> fcall.qid;

	return result
			.then(OkRespose{std::move(fcall)});
}


Result<ResponseMessage, Error>
parseOpenResponse(ByteReader& data) {
    Response::Open fcall;

	Decoder decoder{data};
	Solace::Result<Decoder&, Error> result = (decoder >> fcall.qid >> fcall.iounit);

	return result
			.then(OkRespose{std::move(fcall)});
}


Result<ResponseMessage, Error>
parseCreateResponse(ByteReader& data) {
    Response::Create fcall;

	Decoder decoder{data};
	auto result = decoder >> fcall.qid
						  >> fcall.iounit;
	return result.then(OkRespose{std::move(fcall)});
}


Result<ResponseMessage, Error>
parseReadResponse(ByteReader& data) {
    Response::Read fcall;

	Decoder decoder{data};
	auto result = decoder >> fcall.data;
	return result.then(OkRespose{std::move(fcall)});
}


Result<ResponseMessage, Error>
parseWriteResponse(ByteReader& data) {
    Response::Write fcall;

	Decoder decoder{data};
	auto result = decoder >> fcall.count;
	return result.then(OkRespose{std::move(fcall)});
}


Result<ResponseMessage, Error>
parseStatResponse(ByteReader& data) {
    Response::Stat fcall;

	Decoder decoder{data};
	auto result = decoder >> fcall.dummySize
						  >> fcall.data;
	return result
			.then(OkRespose{std::move(fcall)});
}


Result<ResponseMessage, Error>
parseWalkResponse(ByteReader& data) {
    Response::Walk fcall;

    Decoder decoder{data};

    // FIXME: Non-sense!
	auto result = decoder >> fcall.nqids;
	for (decltype(fcall.nqids) i = 0; i < fcall.nqids && result; ++i) {
		result = decoder >> fcall.qids[i];
	}

	return result
			.then(OkRespose{std::move(fcall)});
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Request parser
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Result<RequestMessage, Error>
parseVersionRequest(ByteReader& data) {
    auto msg = Request::Version{};
	Decoder decoder{data};
	auto result = decoder >> msg.msize
						  >> msg.version;
	return result
			.then(OkRequest(std::move(msg)));
}


Result<RequestMessage, Error>
parseAuthRequest(ByteReader& data) {
    auto msg = Request::Auth{};
	Decoder decoder{data};
	auto result = decoder >> msg.afid
						  >> msg.uname
						  >> msg.aname;

	return result
			.then(OkRequest(std::move(msg)));
}


Result<RequestMessage, Error>
parseFlushRequest(ByteReader& data) {
    auto msg = Request::Flush{};
	Decoder decoder{data};
	auto result = decoder >> msg.oldtag;

	return result
			.then(OkRequest(std::move(msg)));
}


Result<RequestMessage, Error>
parseAttachRequest(ByteReader& data) {
    auto msg = Request::Attach{};
	Decoder decoder{data};
	auto result = decoder >> msg.fid
						  >> msg.afid
						  >> msg.uname
						  >> msg.aname;
	return result
            .then(OkRequest(std::move(msg)));
}


Result<RequestMessage, Error>
parseWalkRequest(ByteReader& data) {
    auto msg = Request::Walk{};
	Decoder decoder{data};
	auto result = decoder >> msg.fid
						  >> msg.newfid
						  >> msg.path;

	return result
			.then(OkRequest(std::move(msg)));
}


Result<RequestMessage, Error>
parseOpenRequest(ByteReader& data) {
    auto msg = Request::Open{};
	Decoder decoder{data};
	auto result = decoder >> msg.fid
						  >> msg.mode.mode;
	return result
            .then(OkRequest(std::move(msg)));
}


Result<RequestMessage, Error>
parseCreateRequest(ByteReader& data) {
    auto msg = Request::Create{};
	Decoder decoder{data};

	auto result = decoder >> msg.fid
						  >> msg.name
						  >> msg.perm
						  >> msg.mode.mode;
	return result
            .then(OkRequest(std::move(msg)));
}


Result<RequestMessage, Error>
parseReadRequest(ByteReader& data) {
    auto msg = Request::Read{};
	Decoder decoder{data};
	auto result = decoder >> msg.fid
						  >> msg.offset
						  >> msg.count;

	return result
			.then(OkRequest(std::move(msg)));
}


Result<RequestMessage, Error>
parseWriteRequest(ByteReader& data) {
    auto msg = Request::Write{};
	Decoder decoder{data};
	auto result = decoder >> msg.fid
						  >> msg.offset
						  >> msg.data;
	return result
			.then(OkRequest(std::move(msg)));
}


Result<RequestMessage, Error>
parseClunkRequest(ByteReader& data) {
    auto msg = Request::Clunk{};
	Decoder decoder{data};
	auto result = decoder >> msg.fid;
	return result
			.then(OkRequest(std::move(msg)));
}


Result<RequestMessage, Error>
parseRemoveRequest(ByteReader& data) {
    auto msg = Request::Remove{};
	Decoder decoder{data};
	auto result = decoder >> msg.fid;
	return result
			.then(OkRequest(std::move(msg)));
}


Result<RequestMessage, Error>
parseStatRequest(ByteReader& data) {
    auto msg = Request::StatRequest{};
	Decoder decoder{data};
	auto result = decoder >> msg.fid;
	return result
			.then(OkRequest(std::move(msg)));
}


Result<RequestMessage, Error>
parseWStatRequest(ByteReader& data) {
    auto msg = Request::WStat{};
	Decoder decoder{data};
	auto result = decoder >> msg.fid
						  >> msg.stat;
	return result
			.then(OkRequest(std::move(msg)));
}



Result<RequestMessage, Error>
parseSessionRequest(ByteReader& data) {
    auto msg = Request_9P2000E::Session{};
	Decoder decoder{data};
	auto result = decoder >> msg.key[0]
			>> msg.key[1]
			>> msg.key[2]
			>> msg.key[3]
			>> msg.key[4]
			>> msg.key[5]
			>> msg.key[6]
			>> msg.key[7];
	return result
			.then(OkRequest(std::move(msg)));
}

Result<RequestMessage, Error>
parseShortReadRequest(ByteReader& data) {
    auto msg = Request_9P2000E::SRead{};
	Decoder decoder{data};
	auto result = decoder >> msg.fid
						  >> msg.path;
	return result
			.then(OkRequest(std::move(msg)));
}

Result<RequestMessage, Error>
parseShortWriteRequest(ByteReader& data) {
    auto msg = Request_9P2000E::SWrite{};
	Decoder decoder{data};
	auto result = decoder >> msg.fid
						  >> msg.path
						  >> msg.data;
	return result
            .then(OkRequest(std::move(msg)));
}

}  // namespace



Result<MessageHeader, Error>
Parser::parseMessageHeader(ByteReader& src) const {
    auto const mandatoryHeaderSize = headerSize();
    auto const dataAvailable = src.remaining();

    // Check that we have enough data to read mandatory message header
    if (dataAvailable < mandatoryHeaderSize) {
		return getCannedError(CannedError::IllFormedHeader);
    }

    Decoder decoder{src};
    MessageHeader header;

	decoder >> header.messageSize;

    // Sanity checks:
    if (header.messageSize < mandatoryHeaderSize) {
		return getCannedError(CannedError::IllFormedHeader_FrameTooShort);
    }

    // It is a serious error if we got a message of a size bigger than negotiated one.
    if (header.messageSize > maxNegotiatedMessageSize()) {
		return getCannedError(CannedError::IllFormedHeader_TooBig);
    }

    // Read message type:
    byte messageBytecode{0};
	decoder >> messageBytecode;
    // don't want any funny messages.
    header.type = static_cast<MessageType>(messageBytecode);
    if (header.type < MessageType::_beginSupportedMessageCode ||
        header.type >= MessageType::_endSupportedMessageCode) {
		return getCannedError(CannedError::UnsupportedMessageType);
    }

    // Read message tag. Tags are provided by the client and can not be checked by the message parser.
    // Unless we are provided with the expected tag...
	decoder >> header.tag;

	return Ok(header);
}


Result<ResponseMessage, Error>
Parser::parseResponse(MessageHeader const& header, ByteReader& data) const {
    auto const expectedData = header.payloadSize();

    // Message data sanity check
    // Just paranoid about huge messages exciding frame size getting through.
    if (header.messageSize > maxNegotiatedMessageSize()) {
		return getCannedError(CannedError::IllFormedHeader_TooBig);
    }

    // Make sure we have been given enough data to read a message as requested in the message size.
	auto const bytesRemaining = data.remaining();
	if (expectedData > bytesRemaining) {
		return getCannedError(CannedError::NotEnoughData);
    }

    // Make sure there is no extra data in the buffer.
	if (expectedData < bytesRemaining) {
		return getCannedError(CannedError::MoreThenExpectedData);
    }

    switch (header.type) {
    case MessageType::RError:   return parseErrorResponse(data);
    case MessageType::RVersion: return parseVersionResponse(data);
    case MessageType::RAuth:    return parseAuthResponse(data);
    case MessageType::RAttach:  return parseAttachResponse(data);
    case MessageType::RWalk:    return parseWalkResponse(data);
    case MessageType::ROpen:    return parseOpenResponse(data);
    case MessageType::RCreate:  return parseCreateResponse(data);
    case MessageType::RSRead:  // Note: RRead is re-used here for RSRead
    case MessageType::RRead:    return parseReadResponse(data);
    case MessageType::RSWrite:  // Note: RWrite is re-used here for RSWrite
    case MessageType::RWrite:   return parseWriteResponse(data);
    case MessageType::RStat:    return parseStatResponse(data);

    // Responses with no data use common procedure:
    case MessageType::RFlush:   return parseNoDataResponse<Response::Flush>();
    case MessageType::RClunk:   return parseNoDataResponse<Response::Clunk>();
    case MessageType::RRemove:  return parseNoDataResponse<Response::Remove>();
    case MessageType::RWStat:   return parseNoDataResponse<Response::WStat>();
    case MessageType::RSession: return parseNoDataResponse<Response_9P2000E::Session>();

    default:
		return getCannedError(CannedError::UnsupportedMessageType);
    }
}

Result<RequestMessage, Error>
Parser::parseRequest(MessageHeader const& header, ByteReader& data) const {
	auto const expectedData = header.payloadSize();

    // Message data sanity check
    // Just paranoid about huge messages exciding frame size getting through.
    if (header.messageSize > maxNegotiatedMessageSize()) {
		return getCannedError(CannedError::IllFormedHeader_TooBig);
    }

    // Make sure we have been given enough data to read a message as requested in the message size.
	auto const bytesRemaining = data.remaining();
	if (expectedData > bytesRemaining) {
		return getCannedError(CannedError::NotEnoughData);
    }

    // Make sure there is no extra unexpected data in the buffer.
	if (expectedData < bytesRemaining) {
		return getCannedError(CannedError::MoreThenExpectedData);
    }

    switch (header.type) {
    case MessageType::TVersion: return parseVersionRequest(data);
    case MessageType::TAuth:    return parseAuthRequest(data);
    case MessageType::TFlush:   return parseFlushRequest(data);
    case MessageType::TAttach:  return parseAttachRequest(data);
    case MessageType::TWalk:    return parseWalkRequest(data);
    case MessageType::TOpen:    return parseOpenRequest(data);
    case MessageType::TCreate:  return parseCreateRequest(data);
    case MessageType::TRead:    return parseReadRequest(data);
    case MessageType::TWrite:   return parseWriteRequest(data);
    case MessageType::TClunk:   return parseClunkRequest(data);
    case MessageType::TRemove:  return parseRemoveRequest(data);
    case MessageType::TStat:    return parseStatRequest(data);
    case MessageType::TWStat:   return parseWStatRequest(data);
    /* 9P2000.e extension messages */
    case MessageType::TSession: return parseSessionRequest(data);
    case MessageType::TSRead:   return parseShortReadRequest(data);
    case MessageType::TSWrite:  return parseShortWriteRequest(data);

    default:
		return getCannedError(CannedError::UnsupportedMessageType);
    }
}


size_type
Parser::maxNegotiatedMessageSize(size_type newMessageSize) {
    assertIndexInRange(newMessageSize, 0, maxPossibleMessageSize() + 1);
    _maxNegotiatedMessageSize = std::min(newMessageSize, maxPossibleMessageSize());

    return _maxNegotiatedMessageSize;
}
