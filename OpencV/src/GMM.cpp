#include "GMM.h"
#include <limits>
#include <iostream>

using namespace std;

Component::Component()
{
	mean = cv::Vec3d::all(0);
	cov = cv::Matx33d::zeros();
	inverseCov = cv::Matx33d::zeros();
	covDeterminant = 0;
	totalPiexelCount = 0;
}

// copy
Component::Component(const cv::Mat &modelComponent)
{
	mean = modelComponent(cv::Rect(0, 0, 3, 1));
	cov = modelComponent(cv::Rect(3, 0, 9, 1)).reshape(1, 3);
	covDeterminant = cv::determinant(cov);
	inverseCov = cov.inv();
}



int Component::getCompPixelCount() const
{
	return this->totalPiexelCount;
}


/*************
* ����˹�ֲ��Բ��������ķ�ʽ���
*/
cv::Mat Component::exportModel() const
{
	cv::Mat meanMat = cv::Mat(mean.t()); // 3*1
	cv::Mat covMat = cv::Mat(cov).reshape(1, 1); // ��Ϊ1ͨ����1��, 9*1
	cv::Mat model;
	cv::hconcat(meanMat, covMat, model); // ����������ˮƽ�ϲ�
	return model;// 12*1
}


GMM::GMM(int componentsCount)
	: components(componentsCount), coefs(componentsCount), totalSampleCount(0)
{
}


/*********
* Model �ǻ�ϸ�˹ģ�ͣ���ʵ��һ��һά�����������ɸ�������ÿ������13������
* ���Ƹ�˹���ģ��
*/
GMM GMM::matToGMM(const cv::Mat &model)
{
	//һ�����صģ�Ψһ��Ӧ����˹ģ�͵Ĳ�����������˵һ����˹ģ�͵Ĳ�������  
	//һ������RGB����ͨ��ֵ����3����ֵ��3*3��Э�������һ��Ȩֵ  
	const int paraNumOfComponent = 3/*mean*/ + 9/*covariance*/ + 1/*component weight*/; // ƽ������Э���Ȩ�أ� ����ģ�Ͳ���������
	if ((model.type() != CV_64FC1) || (model.rows != 1) || (model.cols % paraNumOfComponent != 0))
		CV_Error(cv::Error::StsBadArg, "model must have CV_64FC1 type, rows == 1 and cols == 13*componentsCount");
	int componentCount = model.cols / paraNumOfComponent; // �����������
	GMM result(componentCount);
	for (int i = 0; i < componentCount; i++) {
		cv::Mat componentModel = model(cv::Rect(13*i, 0, paraNumOfComponent, 1));
		result.coefs[i] = componentModel.at<double>(0, 0);
		result.components[i] = Component(componentModel(cv::Rect(1, 0, 12, 1)));
	}
	return result;
}

/******
* ����˹ģ��ת��Ϊ���� (13*n)*1,������ϵ����Э���Ȩ��
*/
cv::Mat GMM::GMMtoMat() const
{
	cv::Mat result;
	for (size_t i = 0; i < components.size(); i++) {
		cv::Mat coefMat(1, 1, CV_64F, cv::Scalar(coefs[i]));
		cv::Mat componentMat = components[i].exportModel();
		cv::Mat combinedMat;
		cv::hconcat(coefMat, componentMat, combinedMat);
		if (result.empty()) {
			result = combinedMat; // 13*1
		}
		else {
			cv::hconcat(result, combinedMat, result); // ƴ�Ӿ���result
		}
	}
	return result;
}


/**********
* ��÷���������
*/
int GMM::getComponentsCount() const
{
	return components.size();
}


//����һ�����أ���color=��B,G,R����άdouble����������ʾ���������GMM��ϸ�˹ģ�͵ĸ��ʡ�  
//Ҳ���ǰ����������������componentsCount����˹ģ�͵ĸ������Ӧ��Ȩֵ�������ӣ�  
//��������ĵĹ�ʽ��10���������res���ء�  
//����൱�ڼ���Gibbs�����ĵ�һ�������ȡ���󣩡�

double GMM::operator()(const cv::Vec3d color) const
{
	double res = 0;
	for (size_t i = 0; i < components.size(); i++) {
		if (coefs[i] > 0)
			res += coefs[i] * components[i](color);
	}
	return res;
}

/****************
* ��Ԫ��˹�ֲ���������ܶ�
*/
double Component::operator()(const cv::Vec3d &color) const
{
	double PI = 3.1415926;
	double n = 3;
	cv::Vec3d diff = color - mean;
	double res = 1.0 / (pow(2 * PI, n / 2)*sqrt(covDeterminant)) * exp(-0.5 * (diff.t() * inverseCov * diff)(0));
	return res;
}

/**************
* �鿴����������5����˹�����е��ĸ�����,��������Ǹ�
*/
int GMM::mostPossibleComponent(const cv::Vec3d color) const
{
	int maxid = 0;
	double maxPossible = 0;
	for (int i = 0; i < components.size(); i++) {
		if (components[i](color) >= maxPossible) {
			maxid = i;
			maxPossible = components[i](color);
		}
	}
	return maxid;
}

/****
* ��ʼ����ϸ�˹ģ�ͣ�����
*/
void GMM::initLearning()
{
	for (int ci = 0; ci < components.size(); ci++) {
		components[ci].initLearning();
		coefs[ci] = 0;
	}
	totalSampleCount = 0;
}

/****
* ��ʼ����˹ģ�ͣ�����
*/
void Component::initLearning()
{
	mean = cv::Vec3d::all(0);
	cov = cv::Matx33d::zeros();
	totalPiexelCount = 0;
}


void Component::addPixel(cv::Vec3d color)
{
	mean += color;
	cov += color * color.t(); // ��ԪЭ����
	totalPiexelCount++;
}

void GMM::addPixel(int ci, const cv::Vec3d color)
{
	components[ci].addPixel(color);
	totalSampleCount++;
}


void GMM::endLearning()
{
	for (int ci = 0; ci < components.size(); ci++) {
		int n = components[ci].getCompPixelCount();
		if (n == 0) {
			coefs[ci] = 0;
		}
		else {
			coefs[ci] = (double)n / totalSampleCount;// Ȩ�ذ��ո�˹ģ�������������������
			components[ci].endLearning();
		}
	}
}


void Component::endLearning()
{
	const double variance = 0.01;	// Adds the white noise to avoid singular covariance matrix.
	mean /= totalPiexelCount;
	cov = (1.0 / totalPiexelCount) * cov;
	cov -= mean * mean.t();
	const double det = cv::determinant(cov);
	if (det <= std::numeric_limits<double>::epsilon()) {
		cov += variance * cv::Matx33d::eye();
	}
	covDeterminant = cv::determinant(cov);
	inverseCov = cov.inv();
}