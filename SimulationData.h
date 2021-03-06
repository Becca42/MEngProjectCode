 #pragma once

#include "Landmark.h"
#include "SimulationData.generated.h"

UCLASS(config = Game)
class USimulationData : public UObject {

	GENERATED_BODY()

	// final transform (pos & location)
	FTransform transform;

	/** final gear */
	int gear;

	/** transform of simulation vehicle at every tick */
	TArray<FTransform> path;

	/** speed of simulation vehicle at every tick */
	TArray<FVector> velocities;

	/** rpm at every tick */
	TArray<float> rpms;

	/** landmarks we should be seeing at each tick (keyed by tick) */
	TMap<int32, TArray<ALandmark*>> landmarks;

	/** runtime start to finish */
	float runtime;

public:

	/** object fields have been set and can be accessed appropriately */
	bool bIsReady;

	/* constructor for SimulationData object */
	USimulationData();

	~USimulationData();

	static USimulationData* MAKE(FTransform tran, int g, TArray<FTransform> path, TArray<FVector> velocities, TArray<float> rpms, TMap<int32, TArray<ALandmark*>> landmarks);

	/* initialize empty object */
	void Initialize(FTransform tran, int g, TArray<FTransform> path, TArray<FVector> velocities, TArray<float> rpms, TMap<int32, TArray<ALandmark*>> landmarks);


	/** initialize specifically for target run data (doesn't care about field like gear etc.)
	  * TODO think about if want to store landmarks for target run */
	void InitializeTarget(FTransform tran, TArray<FTransform> path, TArray<FVector> velocities, TArray<float> rpms, float runtime);


	/** Stores final transform */
	FTransform GetTransform();

	/** Stores sequence of 3D locations sampled at every tick
	 * according to https://www.gps.gov/systems/gps/performance/accuracy/, phone gps is accurate to within a ~4,9m a radius*/
	TArray<FTransform> GetPath();

	//NTODO: investigate the lack of reference return
	/** Returns speed array */
	TArray<FVector> GetVelocities();

	/** Returns rpm array */
	TArray<float> GetRMPValues();

	/** Returns landmarks array
	 * @param tick at which landmarks were seen
	 * @returns TArray<FName> array of landmarks seen at given tick*/
	TArray<ALandmark*> GetLandmarksAtTick(int32 tick);

	bool hasLandmarksAtTick(int32 tick);

	float GetRunTime();

	void SetRunTime(float time);
};