/**
* Copyright (C) 2019-2021 Xilinx, Inc
*
* Licensed under the Apache License, Version 2.0 (the "License"). You may
* not use this file except in compliance with the License. A copy of the
* License is located at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
* WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
* License for the specific language governing permissions and limitations
* under the License.
*/
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <math.h>
#include <sys/time.h>
#include <getopt.h>
#include <xcl2.hpp>

const static struct option long_options[] = {{"path", required_argument, 0, 'p'},
                                             {"device", required_argument, 0, 'd'},
                                             {"iter_cnt", required_argument, 0, 'l'},
                                             {"supported", no_argument, 0, 's'},
                                             {0, 0, 0, 0}};

static void printHelp() {
    std::cout << "usage: %s <options>\n";
    std::cout << "  -p <platform_test_area_path>\n";
    std::cout << "  -d <device> \n";
    std::cout << "  -l <loop_iter_cnt> \n";
    std::cout << "  -s <check_supported>\n";
}

int main(int argc, char** argv) {
    int option_index = 0;
    std::string dev_id = "0";
    std::string test_path;
    std::string iter_cnt = "10000";
    std::string b_file = "/bandwidth.xclbin";
    bool flag_s = false;
    // Commandline
    int c = 0;
    while ((c = getopt_long(argc, argv, "p:d:l:s", long_options, &option_index)) != -1) {
        switch (c) {
            case 'p':
                test_path = optarg;
                break;
            case 'd':
                dev_id = optarg;
                break;
            case 's':
                flag_s = true;
                break;
            case 'l':
                iter_cnt = atoi(optarg);
                break;
            default:
                printHelp();
                return 1;
        }
    }
    if (test_path.empty()) {
        std::cout << "ERROR : please provide the platform test path to -p option\n";
        return EXIT_FAILURE;
    }
    if (flag_s) {
        std::string binaryFile = test_path + b_file;
        std::ifstream infile(binaryFile);
        if (!infile.good()) {
            std::cout << "\nNOT SUPPORTED" << std::endl;
            return EOPNOTSUPP;
        } else {
            std::cout << "\nSUPPORTED" << std::endl;
            return EXIT_SUCCESS;
        }
    }

    int NUM_KERNEL;
    std::string filename = "/platform.json";
    std::string platform_json = test_path + filename;

    try {
        boost::property_tree::ptree loadPtreeRoot;
        boost::property_tree::read_json(platform_json, loadPtreeRoot);
        boost::property_tree::ptree temp;

        temp = loadPtreeRoot.get_child("total_ddr_banks");
        NUM_KERNEL = temp.get_value<int>();
    } catch (const std::exception& e) {
        std::string msg("ERROR: Bad JSON format detected while marshaling build metadata (");
        msg += e.what();
        msg += ").";
        std::cout << msg << std::endl;
    }

    std::string binaryFile = test_path + b_file;
    std::ifstream infile(binaryFile);
    if (!infile.good()) {
        std::cout << "\nNOT SUPPORTED" << std::endl;
        return EOPNOTSUPP;
    }

    cl_int err;
    cl::Context context;
    std::string krnl_name = "bandwidth";
    std::vector<cl::Kernel> krnls(NUM_KERNEL);
    cl::CommandQueue q;

    // OPENCL HOST CODE AREA START
    // get_xil_devices() is a utility API which will find the xilinx
    // platforms and will return list of devices connected to Xilinx platform
    auto devices = xcl::get_xil_devices();
    // read_binary_file() is a utility API which will load the binaryFile
    // and will return the pointer to file buffer.
    auto fileBuf = xcl::read_binary_file(binaryFile);
    cl::Program::Binaries bins{{fileBuf.data(), fileBuf.size()}};

    auto pos = dev_id.find(":");
    cl::Device device;
    if (pos == std::string::npos) {
        uint32_t device_index = stoi(dev_id);
        if (device_index >= devices.size()) {
            std::cout << "The device_index provided using -d flag is outside the range of "
                         "available devices\n";
            return EXIT_FAILURE;
        }
        device = devices[device_index];
    } else {
        if (xcl::is_emulation()) {
            std::cout << "Device bdf is not supported for the emulation flow\n";
            return EXIT_FAILURE;
        }
        device = xcl::find_device_bdf(devices, dev_id);
    }

    // Creating Context and Command Queue for selected Device
    OCL_CHECK(err, context = cl::Context(device, nullptr, nullptr, nullptr, &err));
    OCL_CHECK(err, q = cl::CommandQueue(context, device,
                                        CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &err));
    std::cout << "Trying to program device " << device.getInfo<CL_DEVICE_NAME>() << std::endl;
    cl::Program program(context, {device}, bins, nullptr, &err);
    if (err != CL_SUCCESS) {
        std::cout << "Failed to program device with xclbin file!\n";
    } else {
        std::cout << "Device program successful!\n";
        for (int i = 0; i < NUM_KERNEL; i++) {
            std::string cu_id = std::to_string(i + 1);
            std::string krnl_name_full = krnl_name + ":{" + "bandwidth_" + cu_id + "}";

            printf("Creating a kernel [%s] for CU(%d)\n", krnl_name_full.c_str(), i + 1);

            // Here Kernel object is created by specifying kernel name along with
            // compute unit.
            // For such case, this kernel object can only access the specific
            // Compute unit

            OCL_CHECK(err, krnls[i] = cl::Kernel(program, krnl_name_full.c_str(), &err));
        }
    }

    double max_throughput = 0;
    int reps = stoi(iter_cnt);

    for (uint32_t i = 4 * 1024; i <= 16 * 1024 * 1024; i *= 2) {
        unsigned int DATA_SIZE = i;

        if (xcl::is_emulation()) {
            reps = 2;
            if (DATA_SIZE > 8 * 1024) break;
        }

        unsigned int vector_size_bytes = DATA_SIZE;
        std::vector<unsigned char, aligned_allocator<unsigned char> > input_host(DATA_SIZE);
        std::vector<unsigned char, aligned_allocator<unsigned char> > output_host[NUM_KERNEL];

        for (int i = 0; i < NUM_KERNEL; i++) {
            output_host[i].resize(DATA_SIZE);
        }
        for (uint32_t j = 0; j < DATA_SIZE; j++) {
            input_host[j] = j % 256;
        }

        // Initializing output vectors to zero
        for (int i = 0; i < NUM_KERNEL; i++) {
            std::fill(output_host[i].begin(), output_host[i].end(), 0);
        }

        std::vector<cl::Buffer> input_buffer(NUM_KERNEL);
        std::vector<cl::Buffer> output_buffer(NUM_KERNEL);

        // These commands will allocate memory on the FPGA. The cl::Buffer objects
        // can
        // be used to reference the memory locations on the device.
        // Creating Buffers
        for (int i = 0; i < NUM_KERNEL; i++) {
            OCL_CHECK(err, input_buffer[i] = cl::Buffer(context, CL_MEM_READ_WRITE, vector_size_bytes, nullptr, &err));
            OCL_CHECK(err, output_buffer[i] = cl::Buffer(context, CL_MEM_READ_WRITE, vector_size_bytes, nullptr, &err));
        }

        for (int i = 0; i < NUM_KERNEL; i++) {
            OCL_CHECK(err, err = krnls[i].setArg(0, input_buffer[i]));
            OCL_CHECK(err, err = krnls[i].setArg(1, output_buffer[i]));
            OCL_CHECK(err, err = krnls[i].setArg(2, DATA_SIZE));
            OCL_CHECK(err, err = krnls[i].setArg(3, reps));
        }

        for (int i = 0; i < NUM_KERNEL; i++) {
            OCL_CHECK(err, err = q.enqueueWriteBuffer(input_buffer[i], CL_TRUE, 0, vector_size_bytes, input_host.data(),
                                                      nullptr, nullptr));
            OCL_CHECK(err, err = q.finish());
        }

        std::chrono::high_resolution_clock::time_point timeStart;
        std::chrono::high_resolution_clock::time_point timeEnd;

        timeStart = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < NUM_KERNEL; i++) {
            OCL_CHECK(err, err = q.enqueueTask(krnls[i]));
        }
        q.finish();
        timeEnd = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < NUM_KERNEL; i++) {
            OCL_CHECK(err, err = q.enqueueReadBuffer(output_buffer[i], CL_TRUE, 0, vector_size_bytes,
                                                     output_host[i].data(), nullptr, nullptr));
            OCL_CHECK(err, err = q.finish());
        }

        // check
        for (int i = 0; i < NUM_KERNEL; i++) {
            for (uint32_t j = 0; j < DATA_SIZE; j++) {
                if (output_host[i][j] != input_host[j]) {
                    printf("ERROR : kernel failed to copy entry %i input %i output %i\n", j, input_host[j],
                           output_host[i][j]);
                    return EXIT_FAILURE;
                }
            }
        }

        double usduration;
        double dnsduration;
        double dsduration;
        double bpersec;
        double mbpersec;

        usduration = (double)(std::chrono::duration_cast<std::chrono::nanoseconds>(timeEnd - timeStart).count() / reps);

        dnsduration = (double)usduration;
        dsduration = dnsduration / ((double)1000000000);
        bpersec = (DATA_SIZE * NUM_KERNEL) / dsduration;
        mbpersec = (2 * bpersec) / ((double)1024 * 1024); // For concurrent Read/Write

        if (mbpersec > max_throughput) max_throughput = mbpersec;
    }

    std::cout << "Maximum throughput: " << max_throughput << "MB/s\n";

    std::cout << "TEST PASSED\n";

    return EXIT_SUCCESS;
}
