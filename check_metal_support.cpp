#include <opencv2/opencv.hpp>
#include <iostream>

int main() {
    std::cout << "OpenCV version: " << CV_VERSION << std::endl;
    
    // Check Metal support through OpenCL (Apple's Metal is exposed via OpenCL)
    cv::ocl::setUseOpenCL(true);
    bool hasOpenCL = cv::ocl::haveOpenCL();
    
    if (hasOpenCL) {
        cv::ocl::Context context = cv::ocl::Context::getDefault();
        std::cout << "OpenCL/Metal devices: " << context.ndevices() << std::endl;
        
        for (size_t i = 0; i < context.ndevices(); ++i) {
            cv::ocl::Device device = context.device(i);
            std::cout << "Device " << i << ": " << device.name() << std::endl;
            std::cout << "  Vendor: " << device.vendorName() << std::endl;
            std::cout << "  Type: " << (device.type() == cv::ocl::Device::TYPE_GPU ? "GPU" : "CPU") << std::endl;
            std::cout << "  OpenCL version: " << device.OpenCLVersion() << std::endl;
            std::cout << "  Compute units: " << device.maxComputeUnits() << std::endl;
        }
        
        // Test actual GPU computation
        cv::Mat testMat(1000, 1000, CV_32F);
        cv::randn(testMat, 0, 1);
        
        cv::ocl::setUseOpenCL(true);
        cv::UMat gpuMat = testMat.getUMat(cv::ACCESS_READ);
        
        std::cout << "GPU computation test: ";
        try {
            cv::GaussianBlur(gpuMat, gpuMat, cv::Size(5, 5), 0);
            std::cout << "SUCCESS (GPU accelerated)" << std::endl;
        } catch (...) {
            std::cout << "FAILED" << std::endl;
        }
    } else {
        std::cout << "No OpenCL/Metal support detected" << std::endl;
    }
    
    return 0;
}
