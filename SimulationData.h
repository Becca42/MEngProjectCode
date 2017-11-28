 #pragma once

#include "SimulationData.generated.h"

UCLASS(config = Game)
class USimulationData : public UObject {

	GENERATED_BODY()

		/* TODO decide what features to store
		 * (need clues to make sure/get to right place (is there a correct speed, gear, etc. to worry about)
		 * things that can be controlled (1) steering, (2) throttle <-- keep track of some sequence of these (e.g. every n ticks, continuous input values)
		 * - could do with timer (e.g. every 0.1s take a snapshot of steering and throttle
		 * - could add additional function to input keys and just take a snapshot with keypress
		 * -- TODO find out if possible to have > 1 function for a single keypress? */

	// final transform (pos & location)
	FTransform transform;

	/** final engine rotation */
	float engineRPM;

	/** final velocity */
	FVector velocity;

	/** final gear */
	int gear;

	/** transform of simulation vehicle at every tick */
	TArray<FTransform> path;

	/** speed of simulation vehicle at every tick */
	TArray<FVector> velocities;

	/** rpm at every tick */
	TArray<float> rpms;

	/** landmarks we should be seeing */
	// TODO decide if check every tick or just at end <-- if every tick, need to keep track of *when* we should see a given landmark
	TArray<FName> landmarks;

public:

	/** object fields have been set and can be accessed appropriately */
	bool bIsReady;

	/* constructor for SimulationData object */
	USimulationData();

	static USimulationData* MAKE(FTransform tran, float rpm, FVector vel, int g, TArray<FTransform> path, TArray<FVector> velocities, TArray<float> rpms, TArray<FName> landmarks);

	/** returns an error measure for given model
	 * TODO - more complicated implementation
	 * @param expected object representing expected outcome
	 * @returns float error - linear combination of different in (final) position and velocity */
	float calculateError(USimulationData& expected);

	/** Stores final transform */
	FTransform GetTransform();

	/** Stores final velocity */
	FVector GetVelocity();

	/** Stores sequence of 3D locations sampled at every tick */
	TArray<FTransform>* GetPath();

	/** Returns speed array */
	TArray<FVector> GetVelocities();

	/** Returns rpm array */
	TArray<float>* GetRMPValues();

	/** Returns landmarks array */
	TArray<FName>* GetLandmarks();

};