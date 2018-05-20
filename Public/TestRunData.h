// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "TestRunData.generated.h"

/**
 * TODO better describe class
 * store data from test run used to calculate cost and make adjustment
 */
UCLASS()
class VEHICLEADV3_API UTestRunData : public UObject
{
	GENERATED_BODY()

	float steeringChange;
	float throttleChange;
	FTransform finalTransform;
	float finalRPM;

public:
	bool hitgoal;

	UTestRunData();
	static UTestRunData* MAKE(float steering, float throttle);
	static UTestRunData* MAKE(float steering, float throttle, FVector & end);

	~UTestRunData();

	void Initialize(float steering, float throttle);

	void Initialize(float steering, float throttle, FTransform & finalTransform, float finialRPM);

	/** @returns steering change */
	float GetSteeringChange();

	/** @returns throttle change */
	float GetThrottleChange();

	/** @returns final transform */
	FTransform GetFinalTransform();

	/** @returns final rpm */
	float GetFinalRPM();

};
