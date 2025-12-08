#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#define BUFFER_SIZE 4096

// --- HÀM MỚI: Callback để nhận dữ liệu từ Web Server an toàn ---
size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    char *response_buffer = (char *)userp;

    // Kiểm tra để tránh tràn bộ đệm
    if (realsize >= BUFFER_SIZE) {
        printf("Warning: Response too large, truncating...\n");
        realsize = BUFFER_SIZE - 1;
    }

    // Copy dữ liệu nhận được vào biến response
    memcpy(response_buffer, contents, realsize);
    
    // Thêm ký tự kết thúc chuỗi
    response_buffer[realsize] = 0;

    return realsize;
}

void send_http_request(const char* request, char* response) {
    CURL* curl;
    CURLcode res;

    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Error initializing libcurl\n");
        strcpy(response, "Error initializing libcurl");
        return;
    }

    // Configure the HTTP request
    // LƯU Ý: URL này là ví dụ, bạn nên lấy URL mới từ https://webhook.site để test chính xác nhất
    curl_easy_setopt(curl, CURLOPT_URL, "https://webhook.site/d5ea60ce-050e-47c7-b77c-bd16907f0665");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request);

    // --- PHẦN SỬA LỖI ---
    // Thay vì dùng memcpy trực tiếp, ta dùng hàm write_callback
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    // --------------------

    // Perform the HTTP request
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        sprintf(response, "HTTP request failed: %s", curl_easy_strerror(res));
    }

    curl_easy_cleanup(curl);
}

void read_file_to_buffer(const char* filename, char* buffer) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error opening file: %s\n", filename);
        return;
    }

    size_t bytesRead = fread(buffer, 1, BUFFER_SIZE - 1, file);
    buffer[bytesRead] = '\0';  // Null-terminate the string
    fclose(file);
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    if (world_size < 2) {
        fprintf(stderr, "World size must be at least 2\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    if (world_rank == 0) {
        // --- CLIENT PROCESS ---
        char http_request[BUFFER_SIZE] = {0};
        
        if (argc == 2) {
            // Read from a file
            read_file_to_buffer(argv[1], http_request);
            printf("Client is sending file content: %s\n", http_request);
        } else {
            // Default message input
            printf("Enter a message to send: \n");
            fflush(stdout); // QUAN TRỌNG: Đẩy dòng chữ ra màn hình ngay lập tức
            
            fgets(http_request, BUFFER_SIZE, stdin);
            http_request[strcspn(http_request, "\n")] = 0;  // Remove trailing newline
        }

        MPI_Send(http_request, strlen(http_request) + 1, MPI_CHAR, 1, 0, MPI_COMM_WORLD);

        // Receive HTTP response from proxy
        char http_response[BUFFER_SIZE];
        MPI_Recv(http_response, BUFFER_SIZE, MPI_CHAR, 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("Client received response: %s\n", http_response);

    } else if (world_rank == 1) {
        // --- PROXY PROCESS ---
        char http_request[BUFFER_SIZE];
        MPI_Recv(http_request, BUFFER_SIZE, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        printf("Proxy received request: %s\n", http_request);

        // Forward the request to the HTTP server
        char http_response[BUFFER_SIZE] = {0};
        send_http_request(http_request, http_response);

        // Send the response back to the client
        MPI_Send(http_response, strlen(http_response) + 1, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
    }

    MPI_Finalize();
    return 0;
}
