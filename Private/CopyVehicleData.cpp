// Fill out your copyright notice in the Description page of Project Settings.

#include "CopyVehicleData.h"

UCopyVehicleData::UCopyVehicleData()
{
	/*this->linearVelocity = new FVector;
	this->angularVelocity = new FVector;
	this->startPosition = FTransform::Identity;
	this->gear = -1;
	this->rpm = 0.f;*/
}

UCopyVehicleData* UCopyVehicleData::MAKE(FVector linear, FVector angular)
{
	TSharedPtr<UCopyVehicleData> newvdata(NewObject<UCopyVehicleData>());
	//UCopyVehicleData* newDatum = NewObject<UCopyVehicleData>();
	newvdata->linearVelocity = linear;
	newvdata->angularVelocity = angular;
	newvdata->startPosition = FTransform::Identity;
	newvdata->gear = -1;
	newvdata->rpm = 0.f;
	return newvdata.Get();
}

//NTODO: ptrs??  JK make this an initialize version.  Also get rid of those pointers (linear and angular pass by value)
UCopyVehicleData* UCopyVehicleData::MAKE(FVector linear, FVector angular, FTransform & start, int32 currentGear, float rpm)
{
	TSharedPtr<UCopyVehicleData> newvdata(NewObject<UCopyVehicleData>()); // uses implicit constructor syntax e.g. int two(2);
	//UCopyVehicleData* newDatum = NewObject<UCopyVehicleData>();
	newvdata->linearVelocity = linear;
	newvdata->angularVelocity = angular;
	newvdata->startPosition = start;
	newvdata->gear = currentGear;
	newvdata->rpm = rpm;

	return newvdata.Get();
}

UCopyVehicleData::~UCopyVehicleData()
{

}

void UCopyVehicleData::Initialize(FVector linear, FVector angular, FTransform start, int32 currentGear, float rpm)
{
	this->linearVelocity = linear;
	this->angularVelocity = angular;
	this->startPosition = start;
	this->gear = currentGear;
	this->rpm = rpm;
}

//NTODO:ptrs?
FVector UCopyVehicleData::GetLinearVelocity()
{
	return this->linearVelocity;
}

//NTODO:ptrs?
FVector UCopyVehicleData::GetAngularVelocity()
{
	return this->angularVelocity;
}

FTransform UCopyVehicleData::GetStartPosition()
{
	return this->startPosition;
}

int32 UCopyVehicleData::GetGear()
{
	return this->gear;
}

float UCopyVehicleData::GetRpm()
{
	return this->rpm;
}
