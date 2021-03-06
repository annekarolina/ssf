#ifndef SSF_SERVICES_ADMIN_REQUESTS_STOP_SERVICE_REQUEST_H_
#define SSF_SERVICES_ADMIN_REQUESTS_STOP_SERVICE_REQUEST_H_

#include <cstdint>

#include <fstream>
#include <map>
#include <sstream>
#include <string>

#include <boost/system/error_code.hpp>

#include <msgpack.hpp>

#include <ssf/log/log.h>

#include "core/factories/service_factory.h"

#include "core/factory_manager/service_factory_manager.h"

#include "services/admin/command_factory.h"
#include "services/admin/requests/service_status.h"

namespace ssf {
namespace services {
namespace admin {

template <typename Demux>
class StopServiceRequest {
 private:
  typedef std::map<std::string, std::string> Parameters;

 public:
  StopServiceRequest() {}

  StopServiceRequest(uint32_t unique_id) : unique_id_(unique_id) {}

  enum { command_id = 3, reply_id = 2 };

  static bool RegisterOnReceiveCommand(CommandFactory<Demux>* cmd_factory) {
    return cmd_factory->RegisterOnReceiveCommand(
        command_id, &StopServiceRequest::OnReceive);
  }

  static bool RegisterOnReplyCommand(CommandFactory<Demux>* cmd_factory) {
    return cmd_factory->RegisterOnReplyCommand(command_id,
                                               &StopServiceRequest::OnReply);
  }

  static bool RegisterReplyCommandIndex(CommandFactory<Demux>* cmd_factory) {
    return cmd_factory->RegisterReplyCommandIndex(command_id, reply_id);
  }

  static std::string OnReceive(const std::string& serialized_request,
                               Demux* p_demux, boost::system::error_code& ec) {
    StopServiceRequest<Demux> request;

    try {
      auto obj_handle =
          msgpack::unpack(serialized_request.data(), serialized_request.size());
      auto obj = obj_handle.get();
      obj.convert(request);
    } catch (const std::exception&) {
      SSF_LOG("microservice", warn,
              "[admin] stop service[on receive]: cannot extract request");
      ec.assign(::error::invalid_argument, ::error::get_ssf_category());
      return {};
    }

    auto p_service_factory =
        ServiceFactoryManager<Demux>::GetServiceFactory(p_demux);

    p_service_factory->StopService(request.unique_id());

    SSF_LOG("microservice", debug,
            "[admin] stop service request: service id {}", request.unique_id());

    ec.assign(boost::system::errc::interrupted,
              boost::system::system_category());

    std::stringstream ss;
    std::string result;

    ss << request.unique_id();
    ss >> result;

    return result;
  }

  static std::string OnReply(const std::string& serialized_request,
                             Demux* p_demux,
                             const boost::system::error_code& ec,
                             const std::string& serialized_result) {
    StopServiceRequest<Demux> request;

    try {
      auto obj_handle =
          msgpack::unpack(serialized_request.data(), serialized_request.size());
      auto obj = obj_handle.get();
      obj.convert(request);
    } catch (const std::exception&) {
      // TODO: ec?
      SSF_LOG("microservice", warn,
              "[admin] stop service[on reply]: cannot extract request");
      return {};
    }

    uint32_t id = 0;
    try {
      id = std::stoul(serialized_result);
    } catch (const std::exception&) {
      SSF_LOG("microservice", warn,
              "[admin] stop service request: extract reply id failed");
      return std::string();
    }
    ServiceStatus<Demux> reply(id, 0, ec.value(), Parameters());

    return reply.OnSending();
  }

  std::string OnSending() const {
    std::ostringstream ostrs;
    msgpack::pack(ostrs, *this);
    return ostrs.str();
  }

  uint32_t unique_id() { return unique_id_; }

 public:
  // add msgpack function definitions
  MSGPACK_DEFINE(unique_id_)

 private:
  uint32_t unique_id_;
};

}  // admin
}  // services
}  // ssf

#endif  // SSF_SERVICES_ADMIN_REQUESTS_STOP_SERVICE_REQUEST_H_
