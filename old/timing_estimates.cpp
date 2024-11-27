/**
 * Estimates the speed of various general purpose
 * operations
 */

#include <iostream>
#include <vector>
#include <chrono>
#include <random>

int testCopy() {
    const size_t dataSize = 73 * 1024 * 1024; // MB
    
    std::vector<unsigned char> source(dataSize);
    
    // Random number generator to fill the vector with random data
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(0, 255);

    // Fill the source vector with random data
    for (size_t i = 0; i < dataSize; ++i) {
        source[i] = dis(gen);
    }

    // Step 2: Prepare a destination vector
    std::vector<unsigned char> destination(dataSize);

    // Step 3: Start timing the copy operation
    auto start = std::chrono::high_resolution_clock::now();
    
    // Copy data from source to destination
    std::copy(source.begin(), source.end(), destination.begin());

    // Step 4: Stop timing the copy operation
    auto end = std::chrono::high_resolution_clock::now();

    // Step 5: Calculate the elapsed time
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Step 6: Print the results
    std::cout << "Time taken to copy all data: " << duration.count() << " ms" << std::endl;

    return 0;
}

int testLoop() {
    int payloadSize = 500 * 1024 * 1024; // MB
    int blockSize = 512;

    auto start = std::chrono::high_resolution_clock::now();
    std::vector<unsigned char> payload(payloadSize);
    // std::random_device rd;
    // std::mt19937 gen(rd());
    // std::uniform_int_distribution<int> dis(0, 255);
    for (size_t i = 0; i < payloadSize; ++i) {
        payload[i] = i;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Time taken to initialise: " << duration.count() << " ms" << std::endl;

    // Start the timer
    std::cout << "Loop starting" << std::endl;
    start = std::chrono::high_resolution_clock::now();

    // Simulate the loop you provided
    int blockCnt = 0;
    for (int i = 0; i < payloadSize; i += blockSize) {
        // Simulate retrieving block data
        size_t blockEnd = std::min(i + blockSize, payloadSize);
        std::vector<unsigned char> blockData(payload.begin() + i, payload.begin() + blockEnd);
        blockCnt++;
    }

    // Stop the timer
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Output the time taken
    std::cout << "Time taken to break into blocks: " << duration.count() << " ms" << std::endl;

    return 0;
}

int main() {
    testCopy();
    testLoop();
}

