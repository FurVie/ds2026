syntax = "proto3";

package filetransfer;

service FileTransfer {
    rpc SendFile(FileRequest) returns (FileResponse) {}
    rpc ReceiveFile(FileChunk) returns (Empty) {}
}

message FileRequest {
    string filename = 1;
    bytes content = 2;
}

message FileResponse {
    bool success = 1;
}

message FileChunk {
    bytes content = 1;
}

message Empty {}