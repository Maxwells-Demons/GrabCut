
#include "RGBHistogram.h"


// �������������б���ֱ��ͼ
//RGBHistogram::RGBHistogram(const std::vector<cv::Vec3b> &pixels) {
//	pixelCount = pixels.size();
//	for (int i = 0; i < pixels.size(); i++) {
//		bool exist = histogram.find(pixels[i]) != histogram.end() ? true : false;
//		if (exist) {
//			histogram[pixels[i]] = 1;
//		} else {
//			histogram[pixels[i]] = histogram[pixels[i]] + 1;
//		}
//	}
//}

// �������������б���ֱ��ͼ
void RGBHistogram::update(const std::vector<cv::Vec3b>& pixels) {
	histogram.clear();
	pixelCount = pixels.size();
	for (int i = 0; i < pixels.size(); i++) {
		Pixel pixel(pixels[i]);
		bool exist = histogram.find(pixel) != histogram.end() ? true : false;
		if (exist) {
			histogram[pixel] = 1;
		} else {
			histogram[pixel] = histogram[pixels[i]] + 1;
		}
	}
}

// �������ڴ�ֱ��ͼ�ĸ���
double RGBHistogram::operator()(const cv::Vec3b color) const {
	double p = 0.;
	Pixel pixel(color);
	bool exist = histogram.find(pixel) != histogram.end() ? true : false;
	if (exist) {
		p = histogram.find(pixel)->second * 1.0 / pixelCount;
		if (p < EPS) p = EPS;
	} else {
		p = EPS;
	}
	return p;
}
