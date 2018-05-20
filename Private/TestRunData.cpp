// Fill out your copyright notice in the Description page of Project Settings.

#include "TestRunData.h"

UTestRunData::UTestRunData()
{
	this->steeringChange = 0.f;
	this->throttleChange = 0.f;
}

UTestRunData* UTestRunData::MAKE(float steering, float throttle)
{
	UTestRunData* datum = NewObject<UTestRunData>();
	datum->steeringChange = steering;
	datum->throttleChange = throttle;
	return datum;
}

UTestRunData* UTestRunData::MAKE(float steering, float throttle, FVector & end)
{	
	TSharedPtr<UTestRunData> newtrdata(NewObject<UTestRunData>());
	//UTestRunData* datum = NewObject<UTestRunData>();
	newtrdata->steeringChange = steering;
	newtrdata->throttleChange = throttle;
	return newtrdata.Get();
}

UTestRunData::~UTestRunData()
{
}

void UTestRunData::Initialize(float steering, float throttle)
{
	steeringChange = steering;
	throttleChange = throttle;

}

void UTestRunData::Initialize(float steering, float throttle, FTransform & finalTransform, float finialRPM)
{
	steeringChange = steering;
	throttleChange = throttle;
	finalTransform = finalTransform;
	finalRPM = finalRPM;
}

float UTestRunData::GetSteeringChange()
{
	return steeringChange;
}

float UTestRunData::GetThrottleChange()
{
	return throttleChange;
}

FTransform UTestRunData::GetFinalTransform()
{
	return finalTransform;
}

float UTestRunData::GetFinalRPM()
{
	return finalRPM;
}
