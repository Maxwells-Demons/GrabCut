#pragma once
#include <opencv2/imgproc/imgproc.hpp>
#include <iostream>
#include "GMM.h"
#include "RGBHistogram.h"

enum
{
	GC_WITH_RECT  = 0, 
	GC_WITH_MASK  = 1, 
	GC_CUT        = 2  
};
enum { INIT_WITH_KMEANS = 0, INIT_WITH_EM = 1 };

class GMM;
template <typename captype, typename tcaptype, typename flowtype> class Graph;
class GrabCut2D
{
public:
	void GrabCut( const cv::Mat &img, cv::Mat &mask, cv::Rect rect,
		cv::Mat &bgdModel,cv::Mat &fgdModel,
		int iterCount, int mode );  

	~GrabCut2D(void);
private:
    void initMaskWithRect(cv::Mat &mask, cv::Size size, cv::Rect rect);
    void initGMMs(const cv::Mat &img, const cv::Mat &mask, GMM &fgdGMM, GMM &bgdGMM, int init_with=INIT_WITH_KMEANS);
    void initHistograms(const cv::Mat &img, const cv::Mat &mask, RGBHistogram &fgdHistogram, RGBHistogram &bgdHistogram);
    double computeBeta(const cv::Mat &img);
    void computeEdgeWeights( const cv::Mat& img, cv::Mat& leftW, cv::Mat& upleftW,
                       cv::Mat& upW, cv::Mat& uprightW, double beta, double gamma);
    void assignGMMsComponents( const cv::Mat& img, const cv::Mat& mask,
                          const GMM& bgdGMM, const GMM& fgdGMM, cv::Mat& compIdxs);
    void learnGMMs( const cv::Mat& img, const cv::Mat& mask, const cv::Mat& compIdxs,
                          GMM& bgdGMM, GMM& fgdGMM);
    void buildGraph( const cv::Mat& img, const cv::Mat& mask, const GMM& bgdGMM,                    // 使用ＧＭＭ预测概率
                           const GMM& fgdGMM, double lambda, const cv::Mat& leftW,
                           const cv::Mat& upleftW, const cv::Mat& upW, const cv::Mat& uprightW,
                           Graph<double, double, double>& graph );
    void buildGraph(const cv::Mat& img, const cv::Mat& mask, const RGBHistogram& bgdHistogram,      // 使用直方图预测概率
        const RGBHistogram& fgdHistogram, double lambda, const cv::Mat& leftW,
        const cv::Mat& upleftW, const cv::Mat& upW, const cv::Mat& uprightW,
        Graph<double, double, double>& graph);
    void estimateSegmentation( Graph<double, double, double>& graph, cv::Mat& mask );

};

void initGMMwithEM(cv::InputArray samples, cv::OutputArray labels, int numComponents);