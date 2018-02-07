// Fill out your copyright notice in the Description page of Project Settings.

#include "CopyVehicleData.h"

UCopyVehicleData::UCopyVehicleData()
{
	this->linearVelocity = new FVector;
	this->angularVelocity = new FVector;
	this->startPosition = new FTransform;

}

UCopyVehicleData::UCopyVehicleData(FVector* linear, FVector* angular)
{
	this->linearVelocity = linear;
	this->angularVelocity = angular;
	this->startPosition = new FTransform;
}

UCopyVehicleData::UCopyVehicleData(FVector* linear, FVector* angular, FTransform* start)
{
	this->linearVelocity = linear;
	this->angularVelocity = angular;
	this->startPosition = start;
}

FVector* UCopyVehicleData::GetLinearVelocity()
{
	return this->linearVelocity;
}

FVector* UCopyVehicleData::GetAngularVelocity()
{
	return this->angularVelocity;
}

FTransform* UCopyVehicleData::GetStartPosition()
{
	return this->startPosition;
}
