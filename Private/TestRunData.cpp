// Fill out your copyright notice in the Description page of Project Settings.

#include "TestRunData.h"

UTestRunData::UTestRunData()
{
	this->steeringChange = 0.f;
	this->throttleChange = 0.f;
	this->endPoint = FVector::ZeroVector;
}

UTestRunData* UTestRunData::MAKE(float steering, float throttle)
{
	UTestRunData* datum = NewObject<UTestRunData>();
	datum->steeringChange = steering;
	datum->throttleChange = throttle;
	datum->endPoint = FVector::ZeroVector;
	return datum;
}

UTestRunData* UTestRunData::MAKE(float steering, float throttle, FVector & end)
{
	UTestRunData* datum = NewObject<UTestRunData>();
	datum->steeringChange = steering;
	datum->throttleChange = throttle;
	datum->endPoint = end;
	return datum;
}

UTestRunData::~UTestRunData()
{
}

float UTestRunData::GetSteeringChange()
{
	return steeringChange;
}

float UTestRunData::GetThrottleChange()
{
	return throttleChange;
}

FVector UTestRunData::GetEndPoint()
{
	return endPoint;
}

void UTestRunData::SetEndPoint(FVector & endpoint)
{
	this->endPoint = endpoint;
}
