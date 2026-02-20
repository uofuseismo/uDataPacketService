module;
#include <string>
#include <grpcpp/grpcpp.h>
#include <spdlog/spdlog.h>
#include "uDataPacketServiceAPI/v1/broadcast.grpc.pb.h"


export module AsyncWriter;

namespace UDataPacketService
{

[[nodiscard]]
bool validateSubscriber(const grpc::CallbackServerContext *context,
                        const std::string &accessToken)
{
    if (accessToken.empty()){return true;}
    for (const auto &item : context->client_metadata())
    {   
        if (item.first == "x-custom-auth-token")
        {
            if (item.second == accessToken)
            {
                return true;
            }
        }
    }   
    return false;
}


export
class AsyncSubscribe
{
public:

};

export 
class AsyncSubcribeToAll
{
public:

};

}
