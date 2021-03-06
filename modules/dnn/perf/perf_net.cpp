// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html.
//
// Copyright (C) 2017, Intel Corporation, all rights reserved.
// Third party copyrights are property of their respective owners.

#include "perf_precomp.hpp"
#include "opencv2/core/ocl.hpp"

#include "opencv2/dnn/shape_utils.hpp"

namespace opencv_test {

CV_ENUM(DNNBackend, DNN_BACKEND_DEFAULT, DNN_BACKEND_HALIDE, DNN_BACKEND_INFERENCE_ENGINE)
CV_ENUM(DNNTarget, DNN_TARGET_CPU, DNN_TARGET_OPENCL)

class DNNTestNetwork : public ::perf::TestBaseWithParam< tuple<DNNBackend, DNNTarget> >
{
public:
    dnn::Backend backend;
    dnn::Target target;

    dnn::Net net;

    DNNTestNetwork()
    {
        backend = (dnn::Backend)(int)get<0>(GetParam());
        target = (dnn::Target)(int)get<1>(GetParam());
    }

    void processNet(std::string weights, std::string proto, std::string halide_scheduler,
                    const Mat& input, const std::string& outputLayer,
                    const std::string& framework)
    {
        if (backend == DNN_BACKEND_DEFAULT && target == DNN_TARGET_OPENCL)
        {
#if defined(HAVE_OPENCL)
            if (!cv::ocl::useOpenCL())
#endif
            {
                throw cvtest::SkipTestException("OpenCL is not available/disabled in OpenCV");
            }
        }
        if (backend == DNN_BACKEND_INFERENCE_ENGINE && target == DNN_TARGET_OPENCL)
            throw SkipTestException("Skip OpenCL target of Inference Engine backend");

        randu(input, 0.0f, 1.0f);

        weights = findDataFile(weights, false);
        if (!proto.empty())
            proto = findDataFile(proto, false);
        if (backend == DNN_BACKEND_HALIDE)
        {
            if (halide_scheduler == "disabled")
                throw cvtest::SkipTestException("Halide test is disabled");
            if (!halide_scheduler.empty())
                halide_scheduler = findDataFile(std::string("dnn/halide_scheduler_") + (target == DNN_TARGET_OPENCL ? "opencl_" : "") + halide_scheduler, true);
        }
        if (framework == "caffe")
        {
            net = cv::dnn::readNetFromCaffe(proto, weights);
        }
        else if (framework == "torch")
        {
            net = cv::dnn::readNetFromTorch(weights);
        }
        else if (framework == "tensorflow")
        {
            net = cv::dnn::readNetFromTensorflow(weights, proto);
        }
        else
            CV_Error(Error::StsNotImplemented, "Unknown framework " + framework);

        net.setInput(blobFromImage(input, 1.0, Size(), Scalar(), false));
        net.setPreferableBackend(backend);
        net.setPreferableTarget(target);
        if (backend == DNN_BACKEND_HALIDE)
        {
            net.setHalideScheduler(halide_scheduler);
        }

        MatShape netInputShape = shape(1, 3, input.rows, input.cols);
        size_t weightsMemory = 0, blobsMemory = 0;
        net.getMemoryConsumption(netInputShape, weightsMemory, blobsMemory);
        int64 flops = net.getFLOPS(netInputShape);
        CV_Assert(flops > 0);

        net.forward(outputLayer); // warmup

        std::cout << "Memory consumption:" << std::endl;
        std::cout << "    Weights(parameters): " << divUp(weightsMemory, 1u<<20) << " Mb" << std::endl;
        std::cout << "    Blobs: " << divUp(blobsMemory, 1u<<20) << " Mb" << std::endl;
        std::cout << "Calculation complexity: " << flops * 1e-9 << " GFlops" << std::endl;

        PERF_SAMPLE_BEGIN()
            net.forward();
        PERF_SAMPLE_END()

        SANITY_CHECK_NOTHING();
    }
};


PERF_TEST_P_(DNNTestNetwork, AlexNet)
{
    processNet("dnn/bvlc_alexnet.caffemodel", "dnn/bvlc_alexnet.prototxt",
            "alexnet.yml", Mat(cv::Size(227, 227), CV_32FC3), "prob", "caffe");
}

PERF_TEST_P_(DNNTestNetwork, GoogLeNet)
{
    processNet("dnn/bvlc_googlenet.caffemodel", "dnn/bvlc_googlenet.prototxt",
            "", Mat(cv::Size(224, 224), CV_32FC3), "prob", "caffe");
}

PERF_TEST_P_(DNNTestNetwork, ResNet_50)
{
    processNet("dnn/ResNet-50-model.caffemodel", "dnn/ResNet-50-deploy.prototxt",
            "resnet_50.yml", Mat(cv::Size(224, 224), CV_32FC3), "prob", "caffe");
}

PERF_TEST_P_(DNNTestNetwork, SqueezeNet_v1_1)
{
    processNet("dnn/squeezenet_v1.1.caffemodel", "dnn/squeezenet_v1.1.prototxt",
            "squeezenet_v1_1.yml", Mat(cv::Size(227, 227), CV_32FC3), "prob", "caffe");
}

PERF_TEST_P_(DNNTestNetwork, Inception_5h)
{
    if (backend == DNN_BACKEND_INFERENCE_ENGINE) throw SkipTestException("");
    processNet("dnn/tensorflow_inception_graph.pb", "",
            "inception_5h.yml",
            Mat(cv::Size(224, 224), CV_32FC3), "softmax2", "tensorflow");
}

PERF_TEST_P_(DNNTestNetwork, ENet)
{
    if (backend == DNN_BACKEND_INFERENCE_ENGINE) throw SkipTestException("");
    processNet("dnn/Enet-model-best.net", "", "enet.yml",
            Mat(cv::Size(512, 256), CV_32FC3), "l367_Deconvolution", "torch");
}

PERF_TEST_P_(DNNTestNetwork, SSD)
{
    if (backend == DNN_BACKEND_INFERENCE_ENGINE) throw SkipTestException("");
    processNet("dnn/VGG_ILSVRC2016_SSD_300x300_iter_440000.caffemodel", "dnn/ssd_vgg16.prototxt", "disabled",
            Mat(cv::Size(300, 300), CV_32FC3), "detection_out", "caffe");
}

PERF_TEST_P_(DNNTestNetwork, OpenFace)
{
    if (backend == DNN_BACKEND_HALIDE) throw SkipTestException("");
    processNet("dnn/openface_nn4.small2.v1.t7", "", "",
            Mat(cv::Size(96, 96), CV_32FC3), "", "torch");
}

PERF_TEST_P_(DNNTestNetwork, MobileNet_SSD_Caffe)
{
    processNet("dnn/MobileNetSSD_deploy.caffemodel", "dnn/MobileNetSSD_deploy.prototxt", "",
            Mat(cv::Size(300, 300), CV_32FC3), "detection_out", "caffe");
}

PERF_TEST_P_(DNNTestNetwork, MobileNet_SSD_TensorFlow)
{
    if (backend == DNN_BACKEND_INFERENCE_ENGINE) throw SkipTestException("");
    processNet("dnn/ssd_mobilenet_v1_coco.pb", "ssd_mobilenet_v1_coco.pbtxt", "",
            Mat(cv::Size(300, 300), CV_32FC3), "", "tensorflow");
}

PERF_TEST_P_(DNNTestNetwork, DenseNet_121)
{
    if (backend == DNN_BACKEND_HALIDE) throw SkipTestException("");
    processNet("dnn/DenseNet_121.caffemodel", "dnn/DenseNet_121.prototxt", "",
               Mat(cv::Size(224, 224), CV_32FC3), "", "caffe");
}

PERF_TEST_P_(DNNTestNetwork, OpenPose_pose_coco)
{
    if (backend == DNN_BACKEND_HALIDE) throw SkipTestException("");
    processNet("dnn/openpose_pose_coco.caffemodel", "dnn/openpose_pose_coco.prototxt", "",
               Mat(cv::Size(368, 368), CV_32FC3), "", "caffe");
}

PERF_TEST_P_(DNNTestNetwork, OpenPose_pose_mpi)
{
    if (backend == DNN_BACKEND_HALIDE) throw SkipTestException("");
    processNet("dnn/openpose_pose_mpi.caffemodel", "dnn/openpose_pose_mpi.prototxt", "",
               Mat(cv::Size(368, 368), CV_32FC3), "", "caffe");
}

PERF_TEST_P_(DNNTestNetwork, OpenPose_pose_mpi_faster_4_stages)
{
    if (backend == DNN_BACKEND_HALIDE) throw SkipTestException("");
    // The same .caffemodel but modified .prototxt
    // See https://github.com/CMU-Perceptual-Computing-Lab/openpose/blob/master/src/openpose/pose/poseParameters.cpp
    processNet("dnn/openpose_pose_mpi.caffemodel", "dnn/openpose_pose_mpi_faster_4_stages.prototxt", "",
               Mat(cv::Size(368, 368), CV_32FC3), "", "caffe");
}

PERF_TEST_P_(DNNTestNetwork, opencv_face_detector)
{
    if (backend == DNN_BACKEND_HALIDE ||
        backend == DNN_BACKEND_DEFAULT && target == DNN_TARGET_OPENCL)
        throw SkipTestException("");
    processNet("dnn/opencv_face_detector.caffemodel", "dnn/opencv_face_detector.prototxt", "",
               Mat(cv::Size(300, 300), CV_32FC3), "", "caffe");
}

const tuple<DNNBackend, DNNTarget> testCases[] = {
#ifdef HAVE_HALIDE
    tuple<DNNBackend, DNNTarget>(DNN_BACKEND_HALIDE, DNN_TARGET_CPU),
    tuple<DNNBackend, DNNTarget>(DNN_BACKEND_HALIDE, DNN_TARGET_OPENCL),
#endif
#ifdef HAVE_INF_ENGINE
    tuple<DNNBackend, DNNTarget>(DNN_BACKEND_INFERENCE_ENGINE, DNN_TARGET_CPU),
#endif
    tuple<DNNBackend, DNNTarget>(DNN_BACKEND_DEFAULT, DNN_TARGET_CPU),
    tuple<DNNBackend, DNNTarget>(DNN_BACKEND_DEFAULT, DNN_TARGET_OPENCL)
};

INSTANTIATE_TEST_CASE_P(/*nothing*/, DNNTestNetwork, testing::ValuesIn(testCases));

} // namespace
