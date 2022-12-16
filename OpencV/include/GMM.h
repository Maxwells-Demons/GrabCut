#pragma once
//
// Created by geshuaiqi on 6/1/18.
//

#ifndef GMM_H
#define GMM_H
#include <opencv2/core/core.hpp>
#include <vector>

// ����
class Component
{
public:
	Component();
	Component(const cv::Mat &modelComponent);
	cv::Mat exportModel() const;
	void initLearning();
	void addPixel(cv::Vec3d color);
	void endLearning();
	double operator()(const cv::Vec3d &color) const;
	int getCompPixelCount() const;
private:
	cv::Vec3d mean;			// һ������RGB����ͨ�������������ֵ��3*3��Э�������һ��Ȩֵ
	cv::Matx33d cov;		// Э����
	cv::Matx33d inverseCov; // Э����������
	double covDeterminant;  // Э���������ʽ
	int totalPiexelCount;
};

// ��ϸ�˹ģ��
class GMM
{
public:
	GMM(int componentsCount = 5);
	void initLearning();
	void addPixel(int compID, const cv::Vec3d color);
	void endLearning();
	static GMM matToGMM(const cv::Mat &model);
	cv::Mat GMMtoMat() const;
	int getComponentsCount() const;
	double operator()(const cv::Vec3d color) const;
	int mostPossibleComponent(const cv::Vec3d color) const;

private:
	std::vector<Component> components;
	std::vector<double> coefs; // Pi_k
	int totalSampleCount;

};


#endif //GMM_H