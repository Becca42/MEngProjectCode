// Fill out your copyright notice in the Description page of Project Settings.

#include "CopyVehicleData.h"

UCopyVehicleData::UCopyVehicleData()
{
	this->linearVelocity = new FVector;
	this->angularVelocity = new FVector;
	this->startPosition = FTransform::Identity;
	this->gear = -1;
	this->rpm = 0.f;
}

UCopyVehicleData* UCopyVehicleData::MAKE(FVector* linear, FVector* angular)
{
	UCopyVehicleData* newDatum = NewObject<UCopyVehicleData>();
	newDatum->linearVelocity = linear;
	newDatum->angularVelocity = angular;
	newDatum->startPosition = FTransform::Identity;
	newDatum->gear = -1;
	newDatum->rpm = 0.f;
	return newDatum;
}

UCopyVehicleData* UCopyVehicleData::MAKE(FVector* linear, FVector* angular, FTransform & start, int32 currentGear, float rpm)
{
	UCopyVehicleData* newDatum = NewObject<UCopyVehicleData>();
	newDatum->linearVelocity = linear;
	newDatum->angularVelocity = angular;
	newDatum->startPosition = start;
	newDatum->gear = currentGear;
	newDatum->rpm = rpm;

	return newDatum;
}

UCopyVehicleData::~UCopyVehicleData()
{

}

FVector* UCopyVehicleData::GetLinearVelocity()
{
	return this->linearVelocity;
}

FVector* UCopyVehicleData::GetAngularVelocity()
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
