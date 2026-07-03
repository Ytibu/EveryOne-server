#pragma once
#include "message.pb.h"
#include "message.grpc.pb.h"
#include "singleton.h"
#include <grpcpp/grpcpp.h>
#include "header.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using message::GetVerifyReq;
using message::GetVerifyRsp;
using message::VerifyService;

class VerifyGrpcClient : public Singleton<VerifyGrpcClient>
{
    friend class Singleton<VerifyGrpcClient>;
public:
    GetVerifyRsp GetVerifyCode(std::string email)
    {
        ClientContext context;
        GetVerifyRsp reply;
        GetVerifyReq request;
        request.set_email(email);

        Status status = stub_->GetVerifyCode(&context, request, &reply);
        if (status.ok())
        {
            std::cout << "VerifyGrpcClient: " << reply.email() << std::endl;
        }
        else
        {
            reply.set_error(ErrorCodes::RPCFailed);
        }
        return reply;
    }

private:
    VerifyGrpcClient()
    {
        std::string server_address = "localhost:50051";
        stub_ = VerifyService::NewStub(grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials()));
    }


private:
    std::unique_ptr<VerifyService::Stub> stub_;
};