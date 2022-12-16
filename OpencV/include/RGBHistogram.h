#pragma once

#ifndef _RGBHIST_H_
#define _RGBHIST_H_

#include <opencv2/imgproc/imgproc.hpp>
#include <map>

class Pixel {
public:
	Pixel(const cv::Vec3b& pixel) { bgr = pixel; }
	cv::Vec3b bgr;
	bool operator<(const Pixel& pixel) const {			// 只是为了封装一下，不然没法重载运算符
		if (bgr[0] < pixel.bgr[0]) return true;
		return false;
	}
};

class RGBHistogram {
public:
	RGBHistogram() { pixelCount = 0; }
	//RGBHistogram(const std::vector<cv::Vec3b> &img);
	void update(const std::vector<cv::Vec3b>& pixels);
	double operator()(const cv::Vec3b color) const;		// 返回属于此直方图的概率
	int getPixelCount() { return pixelCount; }
private:
	const double EPS = 1e-8;
	std::map <Pixel, int> histogram;
	int pixelCount;
};


#endif
