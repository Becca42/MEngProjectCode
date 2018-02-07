// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 
 */
class VEHICLEADV3_API TestRunData
{
	float steeringChange;
	float throttleChange;
	FTransform* endPoint;

public:
	TestRunData();
	TestRunData(float steering, float throttle);
	TestRunData(float steering, float throttle, FTransform* end);

	~TestRunData();

	/** @returns steering change */
	float GetSteeringChange();

	/** @returns throttle change */
	float GetThrottleChange();

	/** @returns endpoint */
	FTransform* GetEndPoint();

	/** @param ftransform to set endpoint to */
	void SetEndPoint(FTransform* endpoint);
};
