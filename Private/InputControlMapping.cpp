// Fill out your copyright notice in the Description page of Project Settings.

#include "VehicleAdv3.h"
#include "InputControlMapping.h"




UInputControlMapping::UInputControlMapping()
{
	
}

void UInputControlMapping::init()
{
	
	if (EndTransforms && EndTransforms->Num() > 0)
	{
		return;
	}

	buildTransforms();
	calculateDistances();
}

void UInputControlMapping::buildTransforms()
{
	EndTransforms = new TArray<FTransform>;
	FVector scale(1., 1., 1.);
	for (int i = 0; i < 2090; i++)
	{
		FVector loc(endPOS[i][0], endPOS[i][1], endPOS[i][2]);
		FRotator rot(endROT[i][0], endROT[i][1], endROT[i][2]);
		FTransform* tempTrans = new FTransform(rot, loc, scale);
		//UE_LOG(VehicleRunState, Log, TEXT("EndTransform > %s"), *tempTrans->ToHumanReadableString());
		EndTransforms->Add(*tempTrans);
	}
}

void UInputControlMapping::calculateDistances()
{
	// TODO calculate and store distances for each end transform 
	//so to use would first sample uniformly at random from keys in map then if the key (distance) maps to more than one input set, again sample at random for the values

	distacneMappings = *(new TMap<float, TArray<int>>);

	// loop through array of end transforms and compute distance
	for (int i = 0; i < 2090; i++)
	{
		FTransform endTransform = EndTransforms->operator[](i);
		float distance =FVector::DistSquaredXY(startTransform->GetLocation(), endTransform.GetLocation());

		// bucket distances rounded to 1 decimal place
		//distance = FMath::Round(distance * 100.f)/100.f;
		distance = FMath::Round(distance * 100.f) ;

		// REMOVE - DEBUG
		UE_LOG(VehicleRunState, Log, TEXT("distance: %f"), distance);


		// if distance is not in map already, add
		if (distacneMappings.Contains(distance))
		{
			// REMOVE - DEBUG
			UE_LOG(VehicleRunState, Log, TEXT("first in array: %f"), distacneMappings[distance][0]);

			TArray<int> newArray;
			distacneMappings.RemoveAndCopyValue(distance, newArray);
			newArray.Add(i);
			distacneMappings.Add(distance, newArray);
		}
		// else (if distance in map) append index to controls OR controls <-- decide this) to map
		else
		{
			TArray<int> newArray = *(new TArray<int>());
			newArray.Add(i);
			distacneMappings.Add(distance, newArray);
		}
	}

}
