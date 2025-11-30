#include <iostream>
#include <memory>
#include <string>
#include <fstream>
#include <grpcpp/grpcpp.h>
#include "file_transfer.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using filetransfer::FileTransfer;
using filetransfer::FileRequest;
using filetransfer::FileResponse;
using filetransfer::FileChunk;
using filetransfer::Empty;

class FileTransferServiceImpl final : public FileTransfer::Service {

    Status SendFile(ServerContext* context, const FileRequest* request, FileResponse* response) override {

        std::ofstream file(request->filename(), std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Error opening file for writing" << std::endl;
            return Status::OK;
        }

        file.write(request->content().c_str(), request->content().length());
        file.close();

        response->set_success(true);
        return Status::OK;
    }

    Status ReceiveFile(ServerContext* context, const FileChunk* request, Empty* response) override {

        std::ofstream file("received_file.txt", std::ios::binary | std::ios::app);
        if (!file.is_open()) {
            std::cerr << "Error opening file for writing" << std::endl;
            return Status::OK;
        }

        file.write(request->content().c_str(), request->content().length());
        file.close();
        return Status::OK;
    }
};

void RunServer() {
    std::string server_address("0.0.0.0:50051");
    FileTransferServiceImpl service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;
    server->Wait();
}

int main() {
    RunServer();
    return 0;
}
