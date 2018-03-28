// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "CopyVehicleData.generated.h"

/**
 * 
 */
UCLASS()
class VEHICLEADV3_API UCopyVehicleData : public UObject
{
	GENERATED_BODY()

		//NTODO: pointers?
	FVector linearVelocity;
	FVector angularVelocity;
	FTransform startPosition;
	int32 gear;
	float rpm;

public:

	UCopyVehicleData();
	
	static UCopyVehicleData* MAKE(FVector linear, FVector angular);
	static UCopyVehicleData* MAKE(FVector linear, FVector angular, FTransform & start, int32 currentGear, float rpm);

	~UCopyVehicleData();

	/* init method (what a normal constructor would do) */
	void Initialize(FVector linear, FVector angular, FTransform start, int32 currentGear, float rpm);

	//NTODO: ptrs?
	/** @returns linearVelocity */
	FVector GetLinearVelocity();

	/** @returns angularVelocity */
	FVector GetAngularVelocity();

	/** @returns startPosition */
	FTransform GetStartPosition();

	/** @returns gear */
	int32 GetGear();

	/** @returns rpm */
	float GetRpm();
	
};
