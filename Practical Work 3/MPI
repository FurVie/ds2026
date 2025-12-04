#include <mpi.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    const int ROOT = 0;

    if (size < 2) {
        if (rank == ROOT) {
            std::cerr << "Need at least 2 MPI processes (1 sender + 1 receiver)." << std::endl;
        }
        MPI_Finalize();
        return 1;
    }

    
    std::string filename;
    std::vector<char> nameBuf;
    int nameLen = 0;

    if (rank == ROOT) {
        if (argc < 2) {
            std::cerr << "Usage: mpirun -np <n> ./mpi_file_transfer <file_to_send>\n";
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        filename = argv[1];

        // prepare null-terminated name buffer
        nameBuf.assign(filename.begin(), filename.end());
        nameBuf.push_back('\0');
        nameLen = static_cast<int>(nameBuf.size());
    }

    
    MPI_Bcast(&nameLen, 1, MPI_INT, ROOT, MPI_COMM_WORLD);

    if (rank != ROOT) {
        nameBuf.resize(nameLen);
    }

    
    MPI_Bcast(nameBuf.data(), nameLen, MPI_CHAR, ROOT, MPI_COMM_WORLD);

    if (rank != ROOT) {
        filename = std::string(nameBuf.data());
    }

    
    int fileSize = 0;
    std::vector<char> buffer;

    if (rank == ROOT) {
        std::ifstream in(filename, std::ios::binary);
        if (!in.is_open()) {
            std::cerr << "Rank " << rank << ": cannot open file '" << filename << "' for reading.\n";
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        in.seekg(0, std::ios::end);
        fileSize = static_cast<int>(in.tellg());
        in.seekg(0, std::ios::beg);

        buffer.resize(fileSize);
        if (fileSize > 0) {
            in.read(buffer.data(), fileSize);
        }
        in.close();

        std::cout << "Rank " << rank << ": read " << fileSize
                  << " bytes from '" << filename << "'.\n";
    }

    
    MPI_Bcast(&fileSize, 1, MPI_INT, ROOT, MPI_COMM_WORLD);

    if (fileSize == 0) {
        if (rank != ROOT) {
            std::cerr << "Rank " << rank << ": received file size 0, nothing to write.\n";
        }
        MPI_Finalize();
        return 0;
    }

    
    if (rank != ROOT) {
        buffer.resize(fileSize);
    }

    
    MPI_Bcast(buffer.data(), fileSize, MPI_CHAR, ROOT, MPI_COMM_WORLD);

    
    if (rank != ROOT) {
        std::string outName = "received_rank" + std::to_string(rank) + "_" + filename;
        std::ofstream out(outName, std::ios::binary);
        if (!out.is_open()) {
            std::cerr << "Rank " << rank << ": cannot open '" << outName << "' for writing.\n";
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        out.write(buffer.data(), fileSize);
        out.close();

        std::cout << "Rank " << rank << ": wrote " << fileSize
                  << " bytes to '" << outName << "'.\n";
    }

    MPI_Finalize();
    return 0;
}
