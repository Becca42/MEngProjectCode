// Fill out your copyright notice in the Description page of Project Settings.

#include "TestRunData.h"

TestRunData::TestRunData()
{
	this->steeringChange = 0.f;
	this->throttleChange = 0.f;
	this->endPoint = new FTransform();
}

TestRunData::TestRunData(float steering, float throttle)
{
	this->steeringChange = steering;
	this->throttleChange = throttle;
	this->endPoint = new FTransform();
}

TestRunData::TestRunData(float steering, float throttle, FTransform* end)
{
	this->steeringChange = steering;
	this->throttleChange = throttle;
	this->endPoint = end;
}

TestRunData::~TestRunData()
{
}

float TestRunData::GetSteeringChange()
{
	return steeringChange;
}

float TestRunData::GetThrottleChange()
{
	return throttleChange;
}

FTransform* TestRunData::GetEndPoint()
{
	return endPoint;
}

void TestRunData::SetEndPoint(FTransform* endpoint)
{
	this->endPoint = endpoint;
}
