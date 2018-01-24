#include "SimulationData.h"

USimulationData::USimulationData()
{
	bIsReady = false;
}

USimulationData* USimulationData::MAKE(FTransform transform, int g, TArray<FTransform> path, TArray<FVector> speeds, TArray<float> rpms, TMap<int32, TArray<FName>> landmarks)
{
	USimulationData* newSim = NewObject<USimulationData>();
	newSim->transform = transform;
	newSim->gear = g;
	newSim->path = path;
	newSim->velocities = speeds;
	newSim->rpms = rpms;
	newSim->bIsReady = true;
	newSim->landmarks = landmarks;

	return newSim;
}

float USimulationData::calculateError(USimulationData& expected)
{
	//FTransform trans = this->GetTransform();
	//FVector veldiff = (expected.GetVelocity()) - (this->GetVelocity()); //TODO this won't compile as is
	//veldiff = veldiff.GetAbs();
	//FVector transdiff = expected.GetTransform().GetTranslation() - trans.GetTranslation();
	//transdiff = transdiff.GetAbs();
	//float rotdiff = expected.GetTransform().GetRotation().AngularDistance(trans.GetRotation());

	//return veldiff.Size() + transdiff.Size() + rotdiff;
	return -1.0;
}

FTransform USimulationData::GetTransform()
{
	return this->transform;
}

TArray<FTransform>* USimulationData::GetPath()
{
	return &this->path;
}

TArray<FVector> USimulationData::GetVelocities()
{
	return this->velocities;
}

TArray<float>* USimulationData::GetRMPValues()
{
	return &this->rpms;
}

TArray<FName>* USimulationData::GetLandmarksAtTick(int32 tick)
{
	// TODO add fail case for key not exist?
	return &this->landmarks[tick];
}
