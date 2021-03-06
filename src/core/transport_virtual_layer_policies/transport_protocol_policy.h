#ifndef SSF_CORE_TRANSPORT_VIRTUAL_LAYER_POLICIES_TRANSPORT_PROTOCOL_POLICY_H
#define SSF_CORE_TRANSPORT_VIRTUAL_LAYER_POLICIES_TRANSPORT_PROTOCOL_POLICY_H

#include <cstdint>

#include <functional>
#include <memory>

#include <ssf/log/log.h>

#include <boost/system/error_code.hpp>

#include <ssf/log/log.h>

#include "common/error/error.h"

#include "core/transport_virtual_layer_policies/init_packets/ssf_reply.h"
#include "core/transport_virtual_layer_policies/init_packets/ssf_request.h"

#include "versions.h"

namespace ssf {

template <typename Socket>
class TransportProtocolPolicy {
 private:
  using SocketPtr = std::shared_ptr<Socket>;
  using TransportCb =
      std::function<void(Socket&, const boost::system::error_code&)>;

 public:
  TransportProtocolPolicy() {}

  virtual ~TransportProtocolPolicy() {}

  void DoSSFInitiate(Socket& socket, TransportCb callback) {
    SSF_LOG("transport", debug, "starting SSF protocol");

    uint32_t version = GetVersion();
    auto p_ssf_request = std::make_shared<SSFRequest>(version);

    auto on_write = [this, p_ssf_request, &socket, callback](
        const boost::system::error_code& ec, size_t length) {
      DoSSFValidReceive(p_ssf_request, socket, callback, ec, length);
    };
    boost::asio::async_write(socket, p_ssf_request->const_buffer(), on_write);
  }

  void DoSSFInitiateReceive(Socket& socket, TransportCb callback) {
    auto p_ssf_request = std::make_shared<SSFRequest>();

    auto on_read = [this, p_ssf_request, &socket, callback](
        const boost::system::error_code& ec, size_t length) {
      DoSSFValid(p_ssf_request, socket, callback, ec, length);
    };
    boost::asio::async_read(socket, p_ssf_request->buffer(), on_read);
  }

  void DoSSFValid(SSFRequestPtr p_ssf_request, Socket& socket,
                  TransportCb callback, const boost::system::error_code& ec,
                  size_t length) {
    if (ec) {
      SSF_LOG("transport", error, "SSF version NOT read {}", ec.message());
      socket.get_io_service().post(
          [&socket, ec, callback]() { callback(socket, ec); });
      return;
    }

    uint32_t version = p_ssf_request->version();

    SSF_LOG("transport", trace, "SSF version {}", version);

    if (!IsSupportedVersion(version)) {
      SSF_LOG("transport", error, "SSF version NOT supported {}", version);
      boost::system::error_code result_ec(::error::wrong_protocol_type,
                                          ::error::get_ssf_category());
      socket.get_io_service().post(
          [&socket, result_ec, callback]() { callback(socket, result_ec); });
      return;
    }

    auto p_ssf_reply = std::make_shared<SSFReply>(true);
    auto on_write = [this, p_ssf_reply, &socket, callback](
        const boost::system::error_code& ec, size_t length) {
      DoSSFProtocolFinished(p_ssf_reply, socket, callback, ec, length);
    };
    boost::asio::async_write(socket, p_ssf_reply->const_buffer(), on_write);
  }

  void DoSSFValidReceive(SSFRequestPtr p_ssf_request, Socket& socket,
                         TransportCb callback,
                         const boost::system::error_code& ec, size_t length) {
    if (ec) {
      SSF_LOG("transport", error, "could NOT send the SSF request {}",
              ec.message());
      socket.get_io_service().post(
          [&socket, ec, callback]() { callback(socket, ec); });
      return;
    }

    SSF_LOG("transport", debug, "SSF request sent");

    auto p_ssf_reply = std::make_shared<SSFReply>();

    auto on_read = [this, p_ssf_reply, &socket, callback](
        const boost::system::error_code& ec, size_t length) {
      DoSSFProtocolFinished(p_ssf_reply, socket, callback, ec, length);
    };
    boost::asio::async_read(socket, p_ssf_reply->buffer(), on_read);
  }

  void DoSSFProtocolFinished(SSFReplyPtr p_ssf_reply, Socket& socket,
                             TransportCb callback,
                             const boost::system::error_code& ec,
                             size_t length) {
    if (ec) {
      SSF_LOG("transport", error, "could NOT read SSF reply ", ec.message());
      socket.get_io_service().post(
          [&socket, ec, callback]() { callback(socket, ec); });
      return;
    }
    if (!p_ssf_reply->result()) {
      boost::system::error_code result_ec(::error::wrong_protocol_type,
                                          ::error::get_ssf_category());
      SSF_LOG("transport", error, "SSF reply NOT ok {}", ec.message());
      socket.get_io_service().post(
          [&socket, result_ec, callback]() { callback(socket, result_ec); });
      return;
    }
    SSF_LOG("transport", trace, "SSF reply OK");
    socket.get_io_service().post(
        [&socket, ec, callback]() { callback(socket, ec); });
  }

  uint32_t GetVersion() {
    uint32_t version = versions::major;
    version = version << 8;

    version |= versions::minor;
    version = version << 8;

    version |= versions::transport;
    version = version << 8;

    version |= versions::circuit;

    return version;
  }

  bool IsSupportedVersion(uint32_t input_version) {
    // uint8_t circuit = (input_version & 0x000000FF);
    input_version = input_version >> 8;

    uint8_t transport = (input_version & 0x000000FF);
    input_version = input_version >> 8;

    // uint8_t minor = (input_version & 0x000000FF);
    input_version = input_version >> 8;

    uint8_t major = (input_version & 0x000000FF);

    return (major == versions::major) && (transport == versions::transport);
  }
};

}  // ssf

#endif  // SSF_CORE_TRANSPORT_VIRTUAL_LAYER_POLICIES_TRANSPORT_PROTOCOL_POLICY_H
