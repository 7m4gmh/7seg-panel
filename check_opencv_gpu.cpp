#include <opencv2/opencv.hpp>
#include <opencv2/core/ocl.hpp>
#include <iostream>

int main() {
    std::cout << "OpenCV version: " << CV_VERSION << std::endl;
    
    // OpenCL support check
    cv::ocl::setUseOpenCL(true);
    bool hasOpenCL = cv::ocl::haveOpenCL();
    std::cout << "OpenCL support: " << (hasOpenCL ? "YES" : "NO") << std::endl;
    
    if (hasOpenCL) {
        cv::ocl::Context context = cv::ocl::Context::getDefault();
        std::cout << "OpenCL devices: " << context.ndevices() << std::endl;
        
        for (size_t i = 0; i < context.ndevices(); ++i) {
            cv::ocl::Device device = context.device(i);
            std::cout << "Device " << i << ": " << device.name() << std::endl;
            std::cout << "  Type: " << (device.type() == cv::ocl::Device::TYPE_GPU ? "GPU" : "CPU") << std::endl;
        }
    }
    
    // Check for CUDA support (if available)
    #ifdef HAVE_CUDA
        std::cout << "CUDA support: YES" << std::endl;
    #else
        std::cout << "CUDA support: NO" << std::endl;
    #endif
    
    return 0;
}
