// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "TestRunData.generated.h"

/**
 * 
 */
UCLASS()
class VEHICLEADV3_API UTestRunData : public UObject
{
	GENERATED_BODY()

	float steeringChange;
	float throttleChange;
	FVector endPoint;

public:
	UTestRunData();
	static UTestRunData* MAKE(float steering, float throttle);
	static UTestRunData* MAKE(float steering, float throttle, FVector & end);

	~UTestRunData();

	/** @returns steering change */
	float GetSteeringChange();

	/** @returns throttle change */
	float GetThrottleChange();

	/** @returns endpoint */
	FVector GetEndPoint();

	/** @param FVector to set endpoint to */
	void SetEndPoint(FVector & endpoint);
};
