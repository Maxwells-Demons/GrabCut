#include <chrono>
#include <opencv2/imgproc/imgproc.hpp>
#include "GrabCut.h"
#include "GMM.h"
#include "RGBHistogram.h"
#include "graph.h"
#include <opencv2/ml.hpp>
#include <time.h>
using namespace std;

#define USE_GMM true

GrabCut2D::~GrabCut2D(void)
{
}

//һ.�������ͣ�
//���룺
//cv::InputArray _img,     :�����colorͼ��(����-cv:Mat)
//cv::Rect rect            :��ͼ���ϻ��ľ��ο�����-cv:Rect) 
//int iterCount :           :ÿ�ηָ�ĵ�������������-int)


//�м����
//cv::InputOutputArray _bgdModel ��   ����ģ�ͣ��Ƽ�GMM)������-13*n�������������double���͵��Զ������ݽṹ������Ϊcv:Mat������Vector/List/����ȣ�
//cv::InputOutputArray _fgdModel :    ǰ��ģ�ͣ��Ƽ�GMM) ������-13*n�������������double���͵��Զ������ݽṹ������Ϊcv:Mat������Vector/List/����ȣ�


//���:
//cv::InputOutputArray _mask  : ����ķָ��� (���ͣ� cv::Mat)

//��. α�������̣�
//1.Load Input Image: ����������ɫͼ��;
//2.Init Mask: �þ��ο��ʼ��Mask��Labelֵ��ȷ��������0�� ȷ��ǰ����1�����ܱ�����2������ǰ����3��,���ο���������Ϊȷ�����������ο���������Ϊ����ǰ��;
//3.Init GMM: ���岢��ʼ��GMM
//4.Sample Points:ǰ������ɫ���������о��ࣨ������kmeans���������෽��Ҳ��)
//5.Learn GMM(���ݾ������������ÿ��GMM����еľ�ֵ��Э����Ȳ�����
//6.Construct Graph������t-weight(�������n-weight��ƽ�����
//7.Estimate Segmentation(����maxFlow����зָ�)
//8.Save Result�������������mask�������mask��ǰ�������Ӧ�Ĳ�ɫͼ�񱣴����ʾ�ڽ��������У�

void GrabCut2D::GrabCut(const cv::Mat &img, cv::Mat &mask, cv::Rect rect,
                        cv::Mat &bgdModel, cv::Mat &fgdModel,
                        int iterCount, int mode)
{
	if (iterCount <= 0)
		return;

	clock_t start, end;
	start = clock();

    if (USE_GMM) {
        // 3.Init GMM : ���岢��ʼ��GMM
        GMM bgdGMM, fgdGMM; // ǰ����˹ģ���뱳����˹ģ��

        if (mode == GC_WITH_RECT || mode == GC_WITH_MASK) {
            if (mode == GC_WITH_RECT) {
                initMaskWithRect(mask, img.size(), rect); // ����ͼ��
            }
            initGMMs(img, mask, bgdGMM, fgdGMM, INIT_WITH_KMEANS);
        }
        else if (mode == GC_CUT)
        {
            bgdGMM = GMM::matToGMM(bgdModel); // copy ��˹ģ��
            fgdGMM = GMM::matToGMM(fgdModel);
        }


        //6.Construct Graph������t-weight(�������n-weight��ƽ�����
        cv::Mat leftW, upleftW, upW, uprightW;
        const double gamma = 50;
        const double lambda = 9 * gamma;
        const double beta = computeBeta(img);
        computeEdgeWeights(img, leftW, upleftW, upW, uprightW, beta, gamma);
        int vertexCount = img.cols * img.rows;// ������
        int edgeCount = 2 * (4 * img.cols * img.rows - 3 * (img.cols + img.rows) + 2); // ����
        cv::Mat gaussComponentIdMask(img.size(), CV_32SC1); // ��¼ÿ�����صĸ�˹�����Ķ�Ӧ���

        for (int i = 0; i < iterCount; i++) {
            Graph<double, double, double> graph(vertexCount, edgeCount);
            // ����һ���µ�mask����ÿ�����ض�Ӧ�ĸ�˹����,����gaussComponentIdMask
            assignGMMsComponents(img, mask, bgdGMM, fgdGMM, gaussComponentIdMask);
            learnGMMs(img, mask, gaussComponentIdMask, bgdGMM, fgdGMM); // ����mask��ѧ��һ���ϸ�˹ģ��ϵ��

#ifdef _DEBUG
#include <opencv2\opencv.hpp>

            cv::Mat display;
            gaussComponentIdMask.convertTo(display, CV_8U);
            cv::imshow("mask0", display * 50);
            assignGMMsComponents(img, mask, bgdGMM, fgdGMM, gaussComponentIdMask);
            gaussComponentIdMask.convertTo(display, CV_8U);
            cv::imshow("mask1", display * 50);
#endif
            buildGraph(img, mask, bgdGMM, fgdGMM, lambda, leftW, upleftW, upW, uprightW, graph);
            //7.Estimate Segmentation(����maxFlow����зָ�)
            estimateSegmentation(graph, mask);// ����maxflow������и����mask��������mask
        }

        //8.Save Result�������������mask�������mask��ǰ�������Ӧ�Ĳ�ɫͼ�񱣴����ʾ�ڽ��������У�
        fgdModel = fgdGMM.GMMtoMat(); // ���¸�˹ģ��
        bgdModel = bgdGMM.GMMtoMat();
    } else {
        // ʹ��ֱ��ͼ
        RGBHistogram fgdHistogram, bgdHistogram;
        if (mode == GC_WITH_RECT) {
            initMaskWithRect(mask, img.size(), rect); // ����ͼ��
        }
        initHistograms(img, mask, fgdHistogram, bgdHistogram);

        //for (auto i = fgdHistogram.histogram.begin(); i != fgdHistogram.histogram.end(); i++) {
        //    std::cout << i->first.bgr << endl;
        //    std::cout << i->second << endl;
        //    std::cout << fgdHistogram(i->first.bgr) << ' ' << i->second * 1.0 / fgdHistogram.pixelCount << endl;
        //}
        //std::cout << fgdHistogram.pixelCount << endl;

        //std::cout << fgdHistogram(img.at<cv::Vec3b>(0, 0)) << endl;
        //std::cout << img.at<cv::Vec3b>(0, 0) << endl;
        //std::cout << bgdHistogram(img.at<cv::Vec3b>(0, 0)) << endl;


        //6.Construct Graph������t-weight(�������n-weight��ƽ�����
        cv::Mat leftW, upleftW, upW, uprightW;
        const double gamma = 50;
        const double lambda = 9 * gamma;
        const double beta = computeBeta(img);
        computeEdgeWeights(img, leftW, upleftW, upW, uprightW, beta, gamma);
        int vertexCount = img.cols * img.rows;// ������
        int edgeCount = 2 * (4 * img.cols * img.rows - 3 * (img.cols + img.rows) + 2); // ����

        Graph<double, double, double> graph(vertexCount, edgeCount);
        // ����һ���µ�mask����ÿ�����ض�Ӧ�ĸ�˹����,����gaussComponentIdMask
        buildGraph(img, mask, bgdHistogram, fgdHistogram, lambda, leftW, upleftW, upW, uprightW, graph);
        //7.Estimate Segmentation(����maxFlow����зָ�)
        estimateSegmentation(graph, mask);// ����maxflow������и����mask��������mask

    }
    end = clock();
    cout << "������ʱ: " << (double)(end - start)/CLOCKS_PER_SEC << "s\n";
}


void GrabCut2D::initMaskWithRect(cv::Mat &mask, cv::Size size, cv::Rect rect)
{
    mask.create(size, CV_8UC1);
	// ��һ�鶼�Ǳ���
    mask.setTo(cv::GC_BGD);
	// ����󷽿��ڿ�����ǰ��
    rect.x = std::max(0, rect.x);
    rect.y = std::max(0, rect.y);
    rect.width = std::min(rect.width, size.width - rect.x);
    rect.height = std::min(rect.height, size.height - rect.y);
    (mask(rect)).setTo(cv::Scalar(cv::GC_PR_FGD));
}



/*****
* ����������˹ģ�ͺ�ǰ����˹ģ��
* 2.Init Mask: �þ��ο��ʼ��Mask��Labelֵ��ȷ��������0�� ȷ��ǰ����1�����ܱ�����2������ǰ����3��,���ο���������Ϊȷ�����������ο���������Ϊ����ǰ��;
* 3.Init GMM: ���岢��ʼ��GMM(����ģ����ɷָ�Ҳ�ɵõ�����������GMM��ɻ�ӷ֣�
*/
void GrabCut2D::initGMMs(const cv::Mat &img, const cv::Mat &mask, GMM &fgdGMM, GMM &bgdGMM, int init_with)
{
    std::vector<cv::Vec3f> backgroundPixel;
    std::vector<cv::Vec3f> frontgroundPixel;
	cv::Mat bgdLabels;
	cv::Mat fgdLabels;

	vector<cv::Vec3b> meansrecord;


	// ����mask���в��ã��ֱ��ǰ�������ͱ������������������ǵ����أ�
    for (int i = 0; i < img.rows; i++) {
        for (int j = 0; j < img.cols; j++) {
			if (mask.at<uchar>(i, j) == cv::GC_BGD || mask.at<uchar>(i, j) == cv::GC_PR_BGD) {
				backgroundPixel.push_back((cv::Vec3f) img.at<cv::Vec3b>(i, j));
			}
			else {
				frontgroundPixel.push_back((cv::Vec3f) img.at<cv::Vec3b>(i, j));
			}
        }
    }
	// ������ת�ɾ���
    cv::Mat bgdSampleMat(backgroundPixel.size(), 3, CV_32FC1, &backgroundPixel[0][0]);
    cv::Mat fgdSampleMat(frontgroundPixel.size(), 3, CV_32FC1, &frontgroundPixel[0][0]);

    if (init_with == INIT_WITH_KMEANS) {
        // kmeans����
        // double kmeans( InputArray data, int K, InputOutputArray bestLabels, TermCriteria criteria,
        // int attempts, int flags, OutputArray centers = noArray() );
        // 4.Sample Points:ǰ������ɫ���������о��ࣨ������kmeans���������෽��Ҳ��)
        cv::kmeans(bgdSampleMat, bgdGMM.getComponentsCount(), bgdLabels, // 5 components
            cv::TermCriteria(cv::TermCriteria::MAX_ITER, 10, 0.0), 0, cv::KMEANS_PP_CENTERS);
        cv::kmeans(fgdSampleMat, fgdGMM.getComponentsCount(), fgdLabels,
            cv::TermCriteria(cv::TermCriteria::MAX_ITER, 10, 0.0), 0, cv::KMEANS_PP_CENTERS);
    } else {
        // EM�㷨
        initGMMwithEM(bgdSampleMat, bgdLabels, bgdGMM.getComponentsCount());
        initGMMwithEM(fgdSampleMat, fgdLabels, fgdGMM.getComponentsCount());
    }
	// ÿ���㶼����Ӧ��label�����һһ��Ӧ���γɾ��࣬ǰ�󾰸�˹ģ��
	// 5.Learn GMM(���ݾ������������ÿ��GMM����еľ�ֵ��Э����Ȳ�����

    bgdGMM.initLearning();
    for (int i = 0; i < backgroundPixel.size(); i++) {
        bgdGMM.addPixel(bgdLabels.at<int>(i, 0), backgroundPixel[i]); // �ѱ����������뱳��GMM��
    }
    bgdGMM.endLearning();// ѧϰ��ϸ�˹ģ�Ͳ���

    fgdGMM.initLearning();
    for (int i = 0; i < frontgroundPixel.size(); i++) {
        fgdGMM.addPixel(fgdLabels.at<int>(i, 0), frontgroundPixel[i]); // ��ǰ����������ǰ��GMM��
    }
    fgdGMM.endLearning();// ѧϰ��ϸ�˹ģ�Ͳ���
}


void GrabCut2D::initHistograms(const cv::Mat &img, const cv::Mat &mask, RGBHistogram &fgdHistogram, RGBHistogram &bgdHistogram) {
    std::vector<cv::Vec3b> backgroundPixel;
    std::vector<cv::Vec3b> frontgroundPixel;

    // ����mask���в��ã��ֱ��ǰ�������ͱ������������������ǵ����أ�
    for (int i = 0; i < img.rows; i++) {
        for (int j = 0; j < img.cols; j++) {
            if (mask.at<uchar>(i, j) == cv::GC_BGD || mask.at<uchar>(i, j) == cv::GC_PR_BGD) {
                backgroundPixel.push_back(img.at<cv::Vec3b>(i, j));
            }
            else {
                frontgroundPixel.push_back(img.at<cv::Vec3b>(i, j));
            }
        }
    }

    fgdHistogram.update(frontgroundPixel);
    bgdHistogram.update(backgroundPixel);
}


//����beta��Ҳ����Gibbs�������еĵڶ��ƽ����е�ָ�����beta����������  
//�߻��ߵͶԱȶ�ʱ�������������صĲ���Ӱ��ģ������ڵͶԱȶ�ʱ����������  
//���صĲ����ܾͻ�Ƚ�С����ʱ����Ҫ����һ���ϴ��beta���Ŵ�������  
//�ڸ߶Աȶ�ʱ������Ҫ��С����ͱȽϴ�Ĳ��  
//����������Ҫ��������ͼ��ĶԱȶ���ȷ������beta������ļ����Ĺ�ʽ��5����  
/*
Calculate beta - parameter of GrabCut algorithm.
beta = 1/(2*avg(sqr(||color[i] - color[j]||)))
*/
// ֻ��Ҫ�����ĸ�����
double GrabCut2D::computeBeta(const cv::Mat &img)
{
    double beta = 0;
    for (int y = 0; y < img.rows; y++) {
        for (int x = 0; x < img.cols; x++) {
            cv::Vec3d color = img.at<cv::Vec3b>(y, x);
            if (x > 0) // left
            {
                cv::Vec3d diff = color - (cv::Vec3d) img.at<cv::Vec3b>(y, x - 1);
                beta += diff.dot(diff);
            }
            if (y > 0 && x > 0) // upleft
            {
                cv::Vec3d diff = color - (cv::Vec3d) img.at<cv::Vec3b>(y - 1, x - 1);
                beta += diff.dot(diff);
            }
            if (y > 0) // up
            {
                cv::Vec3d diff = color - (cv::Vec3d) img.at<cv::Vec3b>(y - 1, x);
                beta += diff.dot(diff);
            }
            if (y > 0 && x < img.cols - 1) // upright
            {
                cv::Vec3d diff = color - (cv::Vec3d) img.at<cv::Vec3b>(y - 1, x + 1);
                beta += diff.dot(diff);
            }
        }
    }
    if (beta <= std::numeric_limits<double>::epsilon())
        beta = 0;
    else
        beta = 1.f / (2 * beta / (4 * img.cols * img.rows - 3 * img.cols - 3 * img.rows + 2));

    return beta;
}

/************
* ����ߵ�Ȩ��
*/
void GrabCut2D::computeEdgeWeights(const cv::Mat &img, cv::Mat &leftW, cv::Mat &upleftW,
                             cv::Mat &upW, cv::Mat &uprightW, double beta, double gamma)
{
    leftW.create(img.rows, img.cols, CV_64FC1);
    upleftW.create(img.rows, img.cols, CV_64FC1);
    upW.create(img.rows, img.cols, CV_64FC1);
    uprightW.create(img.rows, img.cols, CV_64FC1);
    for (int y = 0; y < img.rows; y++) {
        for (int x = 0; x < img.cols; x++) {
            cv::Vec3d color = img.at<cv::Vec3b>(y, x);
            if (x > 0) // left
            {
                cv::Vec3d diff = color - (cv::Vec3d) img.at<cv::Vec3b>(y, x - 1);
                leftW.at<double>(y, x) = gamma * exp(-beta * diff.dot(diff));
            }
            else
                leftW.at<double>(y, x) = 0;
            if (x > 0 && y > 0) // upleft
            {
                cv::Vec3d diff = color - (cv::Vec3d) img.at<cv::Vec3b>(y - 1, x - 1);
                upleftW.at<double>(y, x) = gamma / sqrt(2.0) * exp(-beta * diff.dot(diff));
            }
            else
                upleftW.at<double>(y, x) = 0;
            if (y > 0) // up
            {
                cv::Vec3d diff = color - (cv::Vec3d) img.at<cv::Vec3b>(y - 1, x);
                upW.at<double>(y, x) = gamma * exp(-beta * diff.dot(diff));
            }
            else
                upW.at<double>(y, x) = 0;
            if (x < img.cols - 1 && y > 0) // upright
            {
                cv::Vec3d diff = color - (cv::Vec3d) img.at<cv::Vec3b>(y - 1, x + 1);
                uprightW.at<double>(y, x) = gamma / sqrt(2.0) * exp(-beta * diff.dot(diff));
            }
            else
                uprightW.at<double>(y, x) = 0;
        }
    }

}

/***************
* ����һ��img��С��mask���Ա��ڴ洢ÿ�����������ĸ���˹����
*/
void GrabCut2D::assignGMMsComponents(const cv::Mat &img, const cv::Mat &mask,
                                     const GMM &bgdGMM, const GMM &fgdGMM, cv::Mat &gaussCompIdMask)
{
    for (int y = 0; y < img.rows; y++) {
        for (int x = 0; x < img.cols; x++) {
            cv::Vec3d color = img.at<cv::Vec3b>(y, x);
			if (mask.at<uchar>(y, x) == cv::GC_BGD || mask.at<uchar>(y, x) == cv::GC_PR_BGD) {
				gaussCompIdMask.at<int>(y, x) = bgdGMM.mostPossibleComponent(color); // ÿ������ָ������
			}
			else {
				gaussCompIdMask.at<int>(y, x) = fgdGMM.mostPossibleComponent(color);
			}
        }
    }
}

// ����Mask�Ľ��������ѵ����˹ģ��
void GrabCut2D::learnGMMs(const cv::Mat &img, const cv::Mat &mask, const cv::Mat &gaussCompIdMask,
                          GMM &bgdGMM, GMM &fgdGMM)
{
    bgdGMM.initLearning();
    fgdGMM.initLearning();
    for (int i = 0; i < bgdGMM.getComponentsCount(); i++) {
        for (int y = 0; y < img.rows; y++) {
            for (int x = 0; x < img.cols; x++) {
                if (gaussCompIdMask.at<int>(y, x) == i) {
                    if (mask.at<uchar>(y, x) == cv::GC_BGD || mask.at<uchar>(y, x) == cv::GC_PR_BGD)
                        bgdGMM.addPixel(i, img.at<cv::Vec3b>(y, x));
                    else
                        fgdGMM.addPixel(i, img.at<cv::Vec3b>(y, x));
                }
            }
        }
    }
    bgdGMM.endLearning();
    fgdGMM.endLearning();
}



/*******************
* ͨ������õ����������ͼ��ͼ�Ķ���Ϊ���ص㣬ͼ�ı��������ֹ��ɣ�  
* һ����ǣ�ÿ��������Sink���t������������Դ��Source������ǰ�������ӵıߣ�  
* ����mask���ж���ǰ����
* �ֱ�ӵ�ͼӱ�
* Construct GCGraph
* ��ͼ
*/
void GrabCut2D::buildGraph(const cv::Mat &img, const cv::Mat &mask, const GMM &bgdGMM,
                                 const GMM &fgdGMM, double lambda, const cv::Mat &leftW,
                                 const cv::Mat &upleftW, const cv::Mat &upW, const cv::Mat &uprightW,
                                 Graph<double, double, double> &graph)
{
    for (int y = 0; y < img.rows; y++) {
        for (int x = 0; x < img.cols; x++) {
            // add node
            int vertexId = graph.add_node();
            cv::Vec3b color = img.at<cv::Vec3b>(y, x);
            // set t-weights
            double fromSource, toSink;
            if (mask.at<uchar>(y, x) == cv::GC_PR_BGD || mask.at<uchar>(y, x) == cv::GC_PR_FGD) {
                fromSource = -log(bgdGMM(color));
                toSink = -log(fgdGMM(color));
            }
            else if (mask.at<uchar>(y, x) == cv::GC_BGD) {
                fromSource = 0;
                toSink = lambda;
            }
            else // GC_FGD
            {
                fromSource = lambda;
                toSink = 0;
            }
            graph.add_tweights(vertexId, fromSource, toSink);

            // set n-weights
			double edgeWeight;
            if (x > 0) { // left
                edgeWeight = leftW.at<double>(y, x);
                graph.add_edge(vertexId, vertexId - 1, edgeWeight, edgeWeight);
            }
            if (x > 0 && y > 0) { // upleft
                edgeWeight = upleftW.at<double>(y, x);
                graph.add_edge(vertexId, vertexId - img.cols - 1, edgeWeight, edgeWeight);
            }
            if (y > 0) { // up
                edgeWeight = upW.at<double>(y, x);
                graph.add_edge(vertexId, vertexId - img.cols, edgeWeight, edgeWeight);
            }
            if (x < img.cols - 1 && y > 0) { // upright
                edgeWeight = uprightW.at<double>(y, x);
                graph.add_edge(vertexId, vertexId - img.cols + 1, edgeWeight, edgeWeight);
            }
        }
    }
}

// ��ֱ��ͼ��Ϊ���ʽ�ͼ
void GrabCut2D::buildGraph(const cv::Mat& img, const cv::Mat& mask, const RGBHistogram& bgdHistogram,
    const RGBHistogram& fgdHistogram, double lambda, const cv::Mat& leftW,
    const cv::Mat& upleftW, const cv::Mat& upW, const cv::Mat& uprightW,
    Graph<double, double, double>& graph)
{
    for (int y = 0; y < img.rows; y++) {
        for (int x = 0; x < img.cols; x++) {
            // add node
            int vertexId = graph.add_node();
            cv::Vec3b color = img.at<cv::Vec3b>(y, x);
            // set t-weights
            double fromSource, toSink;
            if (mask.at<uchar>(y, x) == cv::GC_PR_BGD || mask.at<uchar>(y, x) == cv::GC_PR_FGD) {
                fromSource = -log(bgdHistogram(color));             // ��ʵֻ������ط����ˣ�������GMM��ȫһ��
                toSink = -log(fgdHistogram(color));
            }
            else if (mask.at<uchar>(y, x) == cv::GC_BGD) {
                fromSource = 0;
                toSink = lambda;
            }
            else // GC_FGD
            {
                fromSource = lambda;
                toSink = 0;
            }
            graph.add_tweights(vertexId, fromSource, toSink);

            // set n-weights
            double edgeWeight;
            if (x > 0) { // left
                edgeWeight = leftW.at<double>(y, x);
                graph.add_edge(vertexId, vertexId - 1, edgeWeight, edgeWeight);
            }
            if (x > 0 && y > 0) { // upleft
                edgeWeight = upleftW.at<double>(y, x);
                graph.add_edge(vertexId, vertexId - img.cols - 1, edgeWeight, edgeWeight);
            }
            if (y > 0) { // up
                edgeWeight = upW.at<double>(y, x);
                graph.add_edge(vertexId, vertexId - img.cols, edgeWeight, edgeWeight);
            }
            if (x < img.cols - 1 && y > 0) { // upright
                edgeWeight = uprightW.at<double>(y, x);
                graph.add_edge(vertexId, vertexId - img.cols + 1, edgeWeight, edgeWeight);
            }
        }
    }
}

/****************
* Estimate segmentation using MaxFlow algorithm 
* ͨ��ͼ�ָ�Ľ��������mask��������ͼ��ָ�������������û�ָ��Ϊ��������ǰ��������  
* ��������С�и����mask
* sink ����; source ǰ��
*/
void GrabCut2D::estimateSegmentation(Graph<double, double, double> &graph, cv::Mat &mask)
{
    graph.maxflow();
    for (int y = 0; y < mask.rows; y++) {
        for (int x = 0; x < mask.cols; x++) {
            if (mask.at<uchar>(y, x) == cv::GC_PR_BGD || mask.at<uchar>(y, x) == cv::GC_PR_FGD) {
                if (graph.what_segment(y * mask.cols + x ) == Graph<double, double, double>::SOURCE)
                    mask.at<uchar>(y, x) = cv::GC_PR_FGD;
                else
                    mask.at<uchar>(y, x) = cv::GC_PR_BGD;
            }
        }
    }
}

void initGMMwithEM(cv::InputArray samples, cv::OutputArray labels, int numComponents) {
    cv::Ptr<cv::ml::EM> em_model = cv::ml::EM::create();
    em_model->setClustersNumber(numComponents);
    em_model->setCovarianceMatrixType(cv::ml::EM::COV_MAT_SPHERICAL);
    em_model->setTermCriteria(cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 100, 0.1));
    em_model->trainEM(samples, cv::noArray(), labels, cv::noArray());
}

